#include "session.hpp"
#include "session/transaction.hpp"

#include "../../util/parser.hpp"
#include "../../string.hpp"

namespace fz::http {

namespace {

constexpr std::size_t max_line_size = 4096;
constexpr std::size_t max_headers_count = 100;

}

server::session::session(
	event_handler &target_handler, event_loop &loop, id id, std::unique_ptr<socket> socket, const securable_socket::info *security_info,
	transaction_handler &transaction_handler,
	logger_interface &logger
)
	: tcp::session(target_handler, id, { socket->peer_ip(), socket->address_family() })
	, util::invoker_handler(loop)
	, message_consumer(reqlog_, max_line_size)
	, transaction_handler_(transaction_handler)
	, logger_(logger, "HTTP Session", {{"id", fz::to_string(id)}})
	, reqlog_(logger_, "Request")
	, reslog_(logger_, "Response")
	, socket_(loop, nullptr, std::move(socket), logger_)
	, channel_(*this, 4*128*1024, 5, false, *this)
{
	socket_.set_flags(socket::flag_keepalive);
	socket_.set_keepalive_interval(duration::from_seconds(30));

	shared_transaction_ = std::make_shared<transaction>(event_loop_, *this);

	last_activity_ = monotonic_clock::now();

	if (security_info) {
		security_info_ = *security_info;

		invoke_later([this] {
			channel_.set_buffer_adder(this);
			channel_.set_buffer_consumer(this);
			socket_.set_event_handler(this);

			if (!socket_.make_secure_server(security_info_.min_tls_ver, security_info_.cert, {}, {}, {"http/1.1"})) {
				logger_.log_u(logmsg::error, L"socket_.make_secure_server() failed. Shutting down.");

				channel_.set_socket(&socket_);
				channel_.shutdown(EPROTO);
				return;
			}
		});
	}
	else {
		invoke_later([this] {
			channel_.set_buffer_adder(this);
			channel_.set_buffer_consumer(this);
			channel_.set_socket(&socket_);
		});
	}

}

server::session::~session()
{
	logger_.log_u(logmsg::debug_debug, L"Session destroyed, with ID %d", get_id());
	remove_handler();
	shared_transaction_->detach();
}

void server::session::set_timeouts(const duration &keepalive_timeout, const duration &activity_timeout)
{
	invoke_later([this, keepalive_timeout, activity_timeout] {
		keepalive_timeout_ = keepalive_timeout;
		activity_timeout_ = activity_timeout;

		if (keepalive_timer_id_) {
			keepalive_timer_id_ = stop_add_timer(keepalive_timer_id_, last_activity_ + keepalive_timeout - monotonic_clock::now(), true);
		}
		else {
			if (activity_timeout) {
				activity_timer_id_ = stop_add_timer(activity_timer_id_, last_activity_ + activity_timeout - monotonic_clock::now(), true);
			}
			else {
				stop_timer(activity_timer_id_);
				activity_timer_id_ = {};
			}
		}
	});
}

bool server::session::is_secure() const
{
	return socket_.is_secure();
}

void server::session::on_timer_event(timer_id id)
{
	if (id == keepalive_timer_id_) {
		logger_.log(logmsg::debug_info, L"Keep Alive timeout (%dms) has expired", keepalive_timeout_.get_milliseconds());
		return shutdown(0);
	}
	else
	if (id == activity_timer_id_) {
		auto delta = monotonic_clock::now() - last_activity_;

		if (delta >= activity_timeout_) {
			logger_.log(logmsg::debug_info, L"Activity timeout has expired");

			if (shared_transaction_->response_.status_ == transaction::response::waiting_for_code_and_reason) {
				send_status(408, "Request Timeout") &&
				send_header(headers::Connection, "close") &&
				send_end();
			}
			else {
				shutdown(0);
			}

			return;
		}

		activity_timer_id_ = add_timer(activity_timeout_ - delta, true);
	}
}

void server::session::notify_channel_socket_read_amount(const monotonic_clock &time_point, int64_t)
{
	last_activity_ = time_point;
}

void server::session::notify_channel_socket_written_amount(const monotonic_clock &time_point, int64_t)
{
	last_activity_ = time_point;
}


void server::session::receive_body(badge<server::request>, std::string &&body, std::function<void (std::string, bool)> on_end)
{
	auto &consumer = shared_transaction_->request_.body_writer_.emplace<transaction::string_writer>(std::move(body), std::move(on_end));
	set_body_consumer(consumer);
}

void server::session::receive_body(badge<server::request>, tvfs::file_holder &&file, std::function<void (tvfs::file_holder, bool)> on_end)
{
	auto &consumer = shared_transaction_->request_.body_writer_.emplace<transaction::file_writer>(std::move(file), logger_, std::move(on_end));
	set_body_consumer(consumer);
}

void server::session::on_socket_event(fz::socket_event_source *source, fz::socket_event_flag type, int error)
{
	channel_.set_socket(&socket_);

	if (error && source->root() == socket_.root()) {
		logger_.log_u(logmsg::error, L"Failed securing connection. Reason: %s.", socket_error_description(error));

		channel_.shutdown(error);
		return;
	}

	if (type == socket_event_flag::connection) {
		if (source != source->root() && source->root() == socket_.root()) {
			// All fine, hand the socket down to the channel.
			channel_.set_socket(&socket_);
			return;
		}
	}

	logger_.log_u(logmsg::error,
				  L"We got an unexpected socket_event. is_own_socket [%d], flag [%d], error [%d], from a layer [%d]",
				  source->root() == socket_.root(), type, error, source != source->root());

	channel_.shutdown(EINVAL);
}

void server::session::on_channel_done_event(channel &, channel::error_type error)
{
	target_handler_.send_event<ended_event>(id_, error);
}

bool server::session::is_alive() const
{
	return channel_.get_socket() != nullptr;
}

void server::session::shutdown(int err)
{
	shared_transaction_->request_.waiting_for_consumer_event_ = false;
	channel_.set_buffer_consumer(nullptr);
	channel_.shutdown(err);
}

void server::session::operator ()(const event_base &ev)
{
	if (on_invoker_event(ev))
		return;

	fz::dispatch<
		channel::done_event,
		fz::socket_event,
		timer_event
	>(ev, this,
		&session::on_channel_done_event,
		&session::on_socket_event,
		&session::on_timer_event
	);
}

int server::session::consume_buffer()
{
	if (shared_transaction_must_be_made_) {
		shared_transaction_must_be_made_ = false;
		shared_transaction_ = std::make_shared<transaction>(event_loop_, *this);
	}
	else
	// FIXME: at the moment we do not process a new request if the response is being sent.
	// Doing otherwise would need a temporary buffer to append the new response to, and a queue of responses.
	// We'll do this as a future improvement.
	if (shared_transaction_->request_.got_end_of_message_) {
		// This can only be true if the responder has not finished yet
		shared_transaction_->request_.waiting_for_consumer_event_ = true;
		return EAGAIN;
	}

	return message_consumer::consume_buffer();
}

int server::session::process_message_start_line(std::string_view line)
{
	if (activity_timeout_) {
		activity_timer_id_ = stop_add_timer(std::exchange(keepalive_timer_id_, 0), last_activity_ + activity_timeout_ - monotonic_clock::now(), true);
	}
	else {
		stop_timer(keepalive_timer_id_);
		keepalive_timer_id_ = 0;
	}

	util::parseable_range r(line);

	std::string_view method;
	std::string_view path;
	std::string_view version;

	static constexpr std::string_view http_1_0 = "HTTP/1.0";
	static constexpr std::string_view http_1_1 = "HTTP/1.1";

	bool ok = parse_until_lit(r, method, {' '}) && lit(r, ' ') && parse_until_lit(r, path, {' '}) && lit(r, ' ') && util::parse_until_eol(r, version);
	if (!ok) {
		return process_error(EINVAL, "Malformed message start line.");
	}

	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	if (version == http_1_0) {
		request_.version = request_.version_1_0;
		request_.close_connection_ = true;
		response_.chunked_encoding_is_supported_ = false;
	}
	else
	if (version == http_1_1) {
		request_.version = request_.version_1_1;
		request_.close_connection_ = false;
	}
	else {
		return process_error(EINVAL, "Unsupported HTTP version.");
	}

	if (!request_.uri.parse(path)) {
		return process_error(EINVAL, "Couldn't parse the request target URI.");
	}

	bool must_append_slash = request_.uri.path_.back() == '/';

	// This serves also for path normalization
	request_.uri.path_ = util::fs::absolute_unix_path(std::move(request_.uri.path_), util::fs::unix_format);
	if (request_.uri.path_.empty()) {
		return process_error(EINVAL, "The request target path is invalid");
	}

	if (must_append_slash) {
		// Restore the trailing slash, removed by the above normalization.
		request_.uri.path_ += '/';
	}

	request_.method = method;

	message_consumer::expect_no_body();

	return 0;
}

int server::session::process_message_header(field::name_view name, field::value_view value)
{
	auto &request_ = shared_transaction_->request_;

	if (request_.headers.size() >= max_headers_count) {
		return process_error(EINVAL, "Too many headers.");
	}

	if (name == headers::Connection) {
		if (value.as_list().get("keep-alive")) {
			request_.close_connection_ = false;
		}
		else
		if (value.as_list().get("close")) {
			request_.close_connection_ = true;
		}
		else {
			return process_error(EINVAL, "Unrecognized value for the Connection header");
		}
	}

	auto [it, emplaced] = request_.headers.try_emplace(std::string(name), value);
	if (!emplaced) {
		if (name.str().size() + it->second.str().size() >= max_line_size) {
			return process_error(EINVAL, fz::sprintf("Too many instances of header [%s].", name));
		}

		// FIXME: we should do this only for headers that are defined as comma-separated lists
		it->second.as_list().append(value);
	}

	return 0;
}

int server::session::process_end_of_message_headers()
{
	transaction_handler_.handle_transaction(shared_transaction_);
	return 0;
}


int server::session::process_end_of_message()
{
	auto &request_ = shared_transaction_->request_;

	request_.got_end_of_message_ = true;

	std::visit([&](auto &writer) {
		writer.on_end(true);
	}, request_.body_writer_);

	maybe_accept_next_request();

	return 0;
}

int server::session::process_error(int err, std::string_view msg)
{
	err = message_consumer::process_error(err, msg);

	unsigned int code = 400;
	std::string_view reason = "Bad Request";

	if (err != EINVAL) {
		code = 500;
		reason = "Internal Server Error";
	}

	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	std::visit([&](auto &writer) {
		writer.on_end(false);
	}, request_.body_writer_);

	if (response_.status_ <= transaction::response::waiting_for_code_and_reason) {
		send_status(code, reason) &&
		send_headers({{headers::Connection, "close"}}) &&
		send_end();
	}

	return 0;
}

void server::session::transaction::html_entry_stats::stream_name_to(util::buffer_streamer &bs) const
{
	bs << "<a href=\"" << fz::percent_encode(e_.name());

	if (e_.is_directory()) {
		bs << "/";
	}

	bs << "\">" << fz::html_encoded(e_.name()) << "</a>";
}

void server::session::maybe_accept_next_request()
{
	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	if (request_.got_end_of_message_ && response_.status_ == transaction::response::ended) {
		if (request_.waiting_for_consumer_event_) {
			request_.waiting_for_consumer_event_ = false;
			consumer::send_event(0);
		}


		shared_transaction_->detach();
		shared_transaction_must_be_made_ = true;

		keepalive_timer_id_ = stop_add_timer(std::exchange(activity_timer_id_, 0), keepalive_timeout_, true);
	}
}

/***************************************************************************************************/

auto server::session::stream_headers(std::initializer_list<std::pair<field::name_view, field::value_view>> list) {
	return [this, list](util::buffer_streamer &streamer) {
		for (auto h: list) {
			if (h.first.empty()) {
				continue;
			}

			auto &response_ = shared_transaction_->response_;

			if (h.first == headers::Transfer_Encoding) {
				if (h.second.as_list().last() == "chunked") {
					if (!response_.chunked_encoding_is_supported_) {
						reslog_.log_raw(logmsg::error, L"Chunked transfer encoding is not supported for this response.");
						shutdown(EINVAL);
						return;
					}

					response_.chunked_encoding_requested_ = true;
				}
			}
			else
			if (h.first == headers::Connection && h.second == "close") {
				response_.close_connection_ = true;
			}
			else
			if (h.first == headers::Content_Type) {
				response_.content_type = h.second;
			}

			if (reslog_.should_log(logmsg::debug_debug)) {
				std::string_view log_value
					= h.first == headers::Set_Cookie ?
						"<redacted for privacy>"
					: h.second;
				reslog_.log(logmsg::debug_debug, L"[Status: %d] %s: %s", response_.status_, h.first, log_value);
			}

			streamer << h.first << ": " << h.second << "\r\n";
		}
	};
}

void server::session::flush_headers(body_size_type size_of_body)
{
	auto streamer = buffer_stream();

	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	streamer << std::move(response_.headers_buffer_);

	if (!response_.chunked_encoding_requested_) {
		if (size_of_body == body_size_type(-1)) {
			if (response_.chunked_encoding_is_supported_) {
				streamer << stream_headers({{headers::Transfer_Encoding, "chunked"}});
			}
			else
			if (!response_.close_connection_) {
				streamer << stream_headers({{headers::Connection, "close"}});
			}
		}
		else {
			streamer << stream_headers({{headers::Content_Length, std::to_string(size_of_body)}});
		}
	}

	if (!response_.close_connection_) {
		if (request_.close_connection_) {
			response_.close_connection_ = true;

			streamer << "Connection: close\r\n";
		}
		else
		if (request_.version == request::version_1_0) {
			streamer << "Connection: keep-alive\r\n";
		}
	}

	streamer << "\r\n";

	response_.status_ = transaction::response::waiting_for_body;
}

bool server::session::send_status(unsigned int code, std::string_view reason)
{
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ > response_.status::waiting_for_code_and_reason) {
		reslog_.log_raw(logmsg::error, L"Response code and reason have already been sent.");
		shutdown(EINVAL);
		return false;
	}

	if (reslog_.should_log(logmsg::debug_debug)) {
		reslog_.log(logmsg::debug_debug, L"[Status: %d] HTTP/1.1 %s %s", response_.status_, code, reason);
	}

	if (code == 100) {
		// The 100 Continue response must be sent immediately and doesn't alter the state of the response itself.

		buffer_stream() <<
			"HTTP/1.1 100 " << reason << "\r\n" <<
			"\r\n";
	}
	else {
		util::buffer_streamer(response_.headers_buffer_) <<
			"HTTP/1.1 " << code << " " << reason        << "\r\n" <<
			"Server: " << headers::default_user_agent() << "\r\n" <<
			"Date: "   << datetime::now().get_rfc822()  << "\r\n";

		response_.status_ = transaction::response::status::waiting_for_headers;
	}

	return true;
}

bool server::session::send_headers(std::initializer_list<std::pair<field::name_view, field::value_view> > list)
{
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ < transaction::response::status::waiting_for_headers) {
		reslog_.log_raw(logmsg::error, L"Cannot send headers yet.");
		shutdown(EINVAL);
		return false;
	}

	if (response_.status_ > transaction::response::status::waiting_for_headers) {
		reslog_.log_raw(logmsg::error, L"Headers have already been sent.");
		shutdown(EINVAL);
		return false;
	}

	util::buffer_streamer(response_.headers_buffer_)
		<< stream_headers(list);

	return true;
}

bool server::session::send_body(std::string_view str)
{
	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ < transaction::response::status::waiting_for_headers) {
		reslog_.log_raw(logmsg::error, L"Cannot send body yet.");
		shutdown(EINVAL);
		return false;
	}

	if (!response_.content_type) {
		send_header(headers::Content_Type, "text/plain; charset=utf-8");
	}

	flush_headers(str.size());

	if (request_.method != "HEAD") {
		buffer_stream() << str;

		response_.status_ = transaction::response::status::sent_body;
	}

	return send_end();
}

bool server::session::send_body(tvfs::file_holder file)
{
	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ < transaction::response::status::waiting_for_headers) {
		reslog_.log_raw(logmsg::error, L"Cannot send body yet.");
		shutdown(EINVAL);
		return false;
	}

	if (!response_.content_type) {
		send_header(headers::Content_Type, "application/octet-stream");
	}

	flush_headers(body_size_type(file->size()));

	if (request_.method == "HEAD") {
		send_end();
		return true;
	}

	auto &reader = response_.body_reader_.emplace<transaction::file_reader>(std::move(file), logger_);

	return send_body(reader);
}

bool server::session::send_body(tvfs::entries_iterator it)
{
	auto &request_ = shared_transaction_->request_;
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ < transaction::response::status::waiting_for_headers) {
		reslog_.log_raw(logmsg::error, L"Cannot send body yet.");
		shutdown(EINVAL);
		return false;
	}

	bool format_is_html {};
	bool format_is_ndjson {};

	if (response_.content_type.empty()) {
		format_is_html = true;
		send_header(headers::Content_Type, "text/html; charset=utf-8");
	}
	else
	if (response_.content_type.is("text/html")) {
		format_is_html = true;
	}
	else
	if (response_.content_type.is("application/ndjson")) {
		format_is_ndjson = true;
	}
	else
	if (!response_.content_type.is("text/plain")) {
		reslog_.log_u(logmsg::error, L"Invalid content_type for the directory listing: `%s'.", response_.content_type);
		shutdown(EINVAL);
		return false;
	}

	flush_headers(std::size_t(-1));

	if (request_.method == "HEAD") {
		send_end();
		return true;
	}

	auto &reader = [&]() -> buffer_operator::adder_interface & {
		if (format_is_html) {
			return response_.body_reader_.emplace<transaction::html_entries_reader>(event_loop_, std::move(it));
		}
		else
		if (format_is_ndjson) {
			return response_.body_reader_.emplace<transaction::ndjson_entries_reader>(event_loop_, std::move(it));
		}

		return response_.body_reader_.emplace<transaction::plain_entries_reader>(event_loop_, std::move(it));
	}();

	return send_body(reader);
}

bool server::session::send_body(adder_interface &adder)
{
	auto &response_ = shared_transaction_->response_;

	auto &reader = [&]() -> buffer_operator::adder_interface & {
		if (response_.chunked_encoding_requested_) {
			return response_.body_chunker_.emplace(adder);
		}

		return adder;
	}();

	response_.status_ = transaction::response::status::sending_body;

	process_nested_adder_until_eof(reader, [this](int err) {
		auto &response_ = shared_transaction_->response_;

		if (err) {
			reslog_.log_u(logmsg::error, L"Error while sending body: %d (%s).", err, std::generic_category().message(err));
			return err;
		}

		response_.body_chunker_.reset();
		response_.body_reader_.emplace<transaction::no_reader>();

		response_.status_ = transaction::response::status::sent_body;

		send_end();

		return 0;
	});

	return true;
}

bool server::session::send_end()
{
	auto &response_ = shared_transaction_->response_;

	if (response_.status_ < transaction::response::status::waiting_for_headers) {
		abort_send("Cannot send end of message yet.");
		return false;
	}

	if (response_.status_ == transaction::response::waiting_for_headers) {
		flush_headers(0);
	}

	response_.status_ = transaction::response::status::ended;

	if (response_.close_connection_) {
		shutdown(0);
	}
	else {
		maybe_accept_next_request();
	}

	return true;
}

void server::session::abort_send(std::string_view msg)
{
	reslog_.log_u(logmsg::error, L"ABORTING: %s", msg);
	shutdown(EINVAL);
}

server::session::transaction::html_entries_reader::html_entries_reader(event_loop &loop, tvfs::entries_iterator it)
	: tvfs_entries_lister(loop, it_, "\n")
	, it_(std::move(it))
{
}

void server::session::transaction::html_entries_reader::set_buffer(buffer_operator::locking_buffer *b)
{
	static const char html_prologue[] =
		"<!doctype html>"
			"<html>"
				R"(<head><meta charset="utf-8"/><title>Listing of %s</title></head>)"
				"<body>"
					"<h1>Listing of %s</h1>"
					"<pre>";

	if (b) {
		if (auto l = b->lock()) {
			std::size_t s
				= sizeof(html_prologue)-1 // The prologue size, minus the NUL terminator
				+ it_.name().size()*2     // The two instances of the dir name
				- 2*2                     // The two instances of the format characters for the dir name
				+ 1;                      // The final NUL terminator

			auto m = l->get(s);
			std::snprintf(reinterpret_cast<char*>(m), s, html_prologue, it_.name().c_str(), it_.name().c_str());
			l->add(s-1); // Minus the NUL terminator
		}
	}

	tvfs_entries_lister::set_buffer(b);
}

int server::session::transaction::html_entries_reader::add_to_buffer()
{
	static constexpr std::string_view html_epilogue =
				"</pre>"
			"</body>"
		"</html>";

	int res = tvfs_entries_lister::add_to_buffer();

	if (res == ENODATA) {
		auto b = get_buffer();
		if (!b) {
			return EFAULT;
		}

		b->append(html_epilogue);
	}

	return res;
}

void server::session::transaction::ndjson_entry::operator()(util::buffer_streamer &bs) const
{
	static auto escaped = [](std::string_view s)
	{
		return [s](fz::util::buffer_streamer &bs) {
			for (auto & c : s) {
				switch (c) {
					case '\r':
						bs << "\\r";
						break;
					case '"':
						bs << "\\\"";
						break;
					case '\\':
						bs << "\\\\";
						break;
					case '\n':
						bs << "\\n";
						break;
					case '\t':
						bs << "\\t";
						break;
					case '\b':
						bs << "\\b";
						break;
					case '\f':
						bs << "\\f";
						break;
					default:
						bs << c;
				}
			}
		};
	};

	auto name = escaped(e_.name());
	auto mtime = (e_.mtime()-fz::datetime(0, datetime::milliseconds)).get_milliseconds();
	auto size = e_.size();
	auto type = [t = e_.type()] {
		switch (t) {
			case fz::local_filesys::type::dir: return 'd';
			case fz::local_filesys::type::file: return 'f';
			case fz::local_filesys::type::link: return 'l';
			case fz::local_filesys::type::unknown: return 'u';
		}

		return 'u';
	}();

	bs << "{\"name\":\"" << name << "\",\"mtime\":" << mtime << ",\"type\":\"" << type << "\",\"size\":" << size << "}";
}

/***************************************************************************************************/

}
