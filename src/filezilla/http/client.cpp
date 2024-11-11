#include "client.hpp"
#include "../buffer_operator/line_consumer.hpp"
#include "../util/buffer_streamer.hpp"
#include "../logger/null.hpp"
#include "../logger/type.hpp"
#include "hostaddress.hpp"

namespace fz::http
{

client::client(thread_pool &pool, event_loop &loop, logger_interface &logger, options opts)
	: fz::event_handler(loop)
	, logger::modularized(logger, "HTTP Client")
	, adder()
	, message_consumer(*this, 4096)
	, pool_(pool)
	, logger_(*this)
	, opts_(std::move(opts))
	, channel_(*this, 4*128*1024, 5, false, *this)
{
}

client::~client()
{
	remove_handler();
}

const client::options &client::get_options() const
{
	return opts_;
}

void fz::http::client::enqueue_request(request &&request, response::partial_handler &&handler, fz::duration &&timeout, fz::datetime &&dt, fz::http::client::reqres::flags_type &&flags, fz::address_type &&address_type)
{
	{
		fz::scoped_lock lock(mutex_);

		monotonic_clock at;
		if (dt)
			at = monotonic_clock::now()+(dt-datetime::now());

		reqres_.emplace_back(reqres{std::move(request), std::move(handler), std::move(timeout), std::move(at), std::move(flags), std::move(address_type)});
	}

	process_queue();
}

int client::add_to_buffer()
{
	fz::scoped_lock lock(mutex_);

	if (reqres_.empty())
		return EAGAIN;

	if (current_response_)
		// TODO: pipelining not supported yet.
		return EAGAIN;

	auto &req = current_request_ = std::move(reqres_.front().req);

	if (req.uri.path_.empty())
		req.uri.path_ = "/";

	// Some servers do not expect Host to contain the "standard" port.
	if ((fz::equal_insensitive_ascii(req.uri.scheme_, "https") && req.uri.port_ != 443) || (fz::equal_insensitive_ascii(req.uri.scheme_, "http") && req.uri.port_ != 80))
		req.headers[headers::Host] = join_host_and_port(req.uri.host_, req.uri.port_);
	else
		req.headers[headers::Host] = req.uri.host_;

	req.headers[headers::User_Agent] = opts_.user_agent();
	req.headers[headers::Connection] = "close";

	if (req.body.size() > 0)
		req.headers[headers::Content_Length] = std::to_string(req.body.size());

	for (const auto &d: opts_.default_request_headers()) {
		if (req.headers.find(d.first) == req.headers.end())
			req.headers[d.first] = d.second;
	}

	auto buffer = adder::get_buffer();
	util::buffer_streamer out(*buffer);

	out << req.method << " " << req.uri.get_request() << " HTTP/1.1\r\n";

	for (const auto &h: req.headers) {
		out << h.first << ": " << h.second << "\r\n";
	}

	out << "\r\n";
	out << req.body;

	message_consumer::reset();

	current_response_.emplace();
	current_response_handler_ = std::move(reqres_.front().res_handler);
	current_timeout_ = std::move(reqres_.front().timeout);
	current_flags_ = std::move(reqres_.front().flags);
	current_address_type_ = std::move(reqres_.front().address_type);

	reqres_.pop_front();

	if (!(current_flags_ & reqres::is_confidential))
		logger_.log_u(logmsg::debug_debug, L"***BEGIN REQUEST***\n%s***END REQUEST***", buffer->to_view());

	if (current_timeout_)
		timeout_id_ = add_timer(current_timeout_, true);

	return 0;
}

int client::process_message_start_line(std::string_view line)
{
	if (line.size() < 13 || !fz::equal_insensitive_ascii(line.substr(0, 7), std::string_view("HTTP/1.")))
		return process_error(EINVAL, "Invalid status line in response from server.");

	if (line[9]  < '1' || line[9]  > '5' ||
		line[10] < '0' || line[10] > '9' ||
		line[11] < '0' || line[11] > '9')
	{
		return process_error(EINVAL, "Invalid response code from server.");
	}

	current_response_->code = unsigned((line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0');

	if (current_response_->code == 305)
		return process_error(EINVAL, "Unsupported redirect 305.");

	current_response_->reason = line.substr(13);

	return 0;
}

int client::process_message_header(field::name_view name, field::value_view value)
{
	// FIXME: Cookies must be handled differently, for now just ignore them.
	// See rfc7230§3.2.2
	if (headers::Set_Cookie == name)
		return 0;

	// We don't need to memorize this
	if (headers::Transfer_Encoding == name)
		return 0;

	if (current_response_handler_) {
		auto [it, emplaced] = current_response_->headers.try_emplace(name, value);
		if (!emplaced) {
			it->second.as_list().append(value);
		}

		if (current_response_headers_have_been_parsed_) {
			// If we're here, it means this is actually a trailer, so invoke the handler again
			return current_response_handler_(response::got_headers, *current_response_);
		}
	}

	return 0;
}

int client::process_body_chunk(buffer_string_view chunk)
{
	int res = 0;

	if (current_response_handler_) {
		auto space_left = opts_.max_body_size() - current_response_->body.size();

		if (chunk.size() > space_left) {
			logger_.log_u(logmsg::warning, L"Message body size exceeds the maximum allowed size");

			current_response_->body.append(chunk.data(), space_left);

			return process_end_of_message();
		}

		current_response_->body.append(chunk.data(), chunk.size());

		res = current_response_handler_(response::got_body, *current_response_);
	}

	return res;
}

int client::process_end_of_message_headers()
{
	current_response_headers_have_been_parsed_ = true;

	if (opts_.follow_redirects() && current_response_->code_type() == current_response_->redirect) {
		if (auto it = current_response_->headers.find("Location"); it != current_response_->headers.end()) {
			if (!opts_.redirects_limits() || redirection_num_ < opts_.redirects_limits()) {
				redirection_num_ += 1;

				auto new_uri = fz::uri(it->second);
				new_uri.resolve(current_request_.uri);
				current_request_.uri = std::move(new_uri);

				enqueue_request(std::move(current_request_), std::move(current_response_handler_), std::move(current_timeout_), current_response_->headers.get_retry_at_with_min_delay(0), std::move(current_flags_), std::move(current_address_type_));

				return ECANCELED;
			}
			else {
				logger_.log_u(logmsg::debug_warning, L"Redirection limit %d reached. Not following the redirection.", opts_.redirects_limits());
			}
		}
		else {
			logger_.log_u(logmsg::debug_warning, L"Response code is %s, but new Location was not provided.", current_response_->code_string());
		}
	}

	redirection_num_ = 0;

	if (current_response_handler_)
		return current_response_handler_(response::got_headers, *current_response_);

	return 0;
}

int client::process_end_of_message()
{
	if (timeout_id_) {
		stop_timer(timeout_id_);
		timeout_id_ = {};
	}

	if (current_response_) {
		if (current_response_handler_)
			current_response_handler_(response::got_end, *current_response_);

		current_response_.reset();
		current_response_headers_have_been_parsed_ = false;
	}

	return ECANCELED;
}

int client::process_error(int err, std::string_view msg)
{
	auto err_str = fz::sprintf("%s. %s", fz::socket_error_description(err), msg);
	logger_.log_u(logmsg::debug_warning, L"%s", err_str);

	if (current_response_) {
		current_response_->code = 999;
		current_response_->reason = std::move(err_str);
	}

	return process_end_of_message();
}

void client::notify_channel_socket_read_amount(const monotonic_clock &, int64_t)
{
	if (timeout_id_ && current_timeout_)
		timeout_id_ = stop_add_timer(timeout_id_, current_timeout_, true);
}

void fz::http::client::notify_channel_socket_written_amount(const monotonic_clock &, int64_t)
{
}

void client::process_queue()
{
	fz::scoped_lock lock(mutex_);

	if (!socket_ && reqres_.size() > 0) {
		if (reqres_.front().at && reqres_.front().at && reqres_.front().at > monotonic_clock::now()) {
			// Request must be performed later
			if (!perform_at_id_)
				perform_at_id_ = add_timer(reqres_.front().at);

			return;
		}

		auto &request = reqres_.front().req;
		auto address_type = reqres_.front().address_type;

		if (!fz::equal_insensitive_ascii(request.uri.scheme_, "https") && !fz::equal_insensitive_ascii(request.uri.scheme_, "http") && !request.uri.scheme_.empty()) {
			current_request_ = std::move(request);
			current_response_handler_ = std::move(reqres_.front().res_handler);
			reqres_.pop_front();
			current_response_.emplace();

			process_error(EINVAL, fz::sprintf("Unknown scheme in uri [%s].", current_request_.uri.to_string(false)));
			return;
		}

		must_secure_socket_ = fz::equal_insensitive_ascii(request.uri.scheme_, "https");

		if (!request.uri.port_) {
			request.uri.port_ =  must_secure_socket_ ? 443 : 80;
		}

		if (logger_.should_log(logmsg::debug_debug))  {
			const char *ipv = "";
			if (address_type == fz::address_type::ipv4) {
				ipv = " using IPv4";
			}
			else
			if (address_type == fz::address_type::ipv6) {
				ipv = " using IPv6";
			}

			logger_.log_u(logmsg::debug_debug, L"Connecting to %s%s...", join_host_and_port(request.uri.host_, request.uri.port_), ipv);
		}

		socket_ = std::make_unique<securable_socket>(event_loop_, static_cast<event_handler*>(this), std::make_unique<socket>(pool_, static_cast<event_handler *>(this)), logger::null);

		if (int error = socket_->connect(fz::to_native(fz::to_wstring_from_utf8(request.uri.host_)), request.uri.port_, address_type)) {
			event_handler::send_event<socket_event>(socket_->root(), socket_event_flag::connection, error);
			return;
		}
	}
}

void client::on_connection_error(int error)
{
	fz::scoped_lock lock(mutex_);

	current_request_ = std::move(reqres_.front().req);
	current_response_handler_ = std::move(reqres_.front().res_handler);
	reqres_.pop_front();
	current_response_.emplace();

	lock.unlock();

	error = process_error(error, fz::sprintf("Could not connect to host %s.", join_host_and_port(current_request_.uri.host_, current_request_.uri.port_)));

	lock.lock();

	channel_.set_socket(socket_.get());
	channel_.shutdown(error);
}


void client::on_certificate_verification_event(tls_layer *tls, tls_session_info &info)
{
	bool is_trusted = opts_.cert_verifier() ? opts_.cert_verifier()(info) : info.system_trust();

	tls->set_verification_result(is_trusted);
	logger_.log_u(logmsg::debug_debug, L"Certificate is trusted: %s", is_trusted ? L"yes" : L"no");
}

void client::on_socket_event(socket_event_source *source, socket_event_flag type, int error)
{
	if (error)
		return on_connection_error(error);

	if (type == socket_event_flag::connection) {
		if (must_secure_socket_) {
			if (!socket_->make_secure_client(tls_ver::v1_2, {}, opts_.trust_store(), {}, {"http/1.1"})) {
				logger_.log_u(logmsg::error, L"socket_->make_secure_client() failed. Shutting down.");
				return on_connection_error(ESHUTDOWN);
			}

			must_secure_socket_ = false;
			return;
		}

		channel_.set_buffer_adder(this);
		channel_.set_buffer_consumer(this);
		channel_.set_socket(socket_.get());
		return;
	}

	logger_.log_u(logmsg::error,
				L"We got an unexpected socket_event. is_own_socket [%d], flag [%d], error [%d], from a layer [%d]",
				source->root() == socket_->root(), type, error, source != source->root());

	//assert(false && "We got an unexpected socket_event");
}

void client::on_channel_done_event(channel &, channel::error_type error)
{
	if (error)
		process_error(error.error(), fz::sprintf("Channel closed with error from source %d.", (int)error.source()));
	else
		process_end_of_message();

	socket_.reset();
	process_queue();
}

void client::on_timer(timer_id id)
{
	if (id) {
		fz::scoped_lock lock(mutex_);

		if (id == timeout_id_) {
			timeout_id_ = {};

			auto error = process_error(ETIMEDOUT, "Timed out while waiting for response from the server.");

			channel_.shutdown(error);
		}
		else
		if (id == perform_at_id_) {
			perform_at_id_ = {};

			// Time to perform the action
			process_queue();
		}
	}
}

void client::operator ()(const event_base &ev)
{
	fz::dispatch<
		socket_event,
		channel::done_event,
		certificate_verification_event,
		timer_event
	>(ev, this,
		&client::on_socket_event,
		&client::on_channel_done_event,
		&client::on_certificate_verification_event,
		&client::on_timer
	  );
}

response::partial_handler client::performer::to_partial(response::handler handler)
{
	return [h = std::move(handler)](response::status reason, response &res) {
		if (reason == response::got_end)
			h(std::move(res));

		return 0;
	};
}

}
