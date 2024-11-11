#include "challenger.hpp"

#include "../../util/io.hpp"
#include "../../http/message_consumer.hpp"
#include "../../buffer_operator/streamed_adder.hpp"
#include "../../util/invoke_later.hpp"

namespace fz::acme
{

/**************************************************************/

client::challenger::~challenger(){}

std::string client::challenger::get_jwk_fingerprint(const std::pair<json, json> &jwk)
{
	// As per RFC 7638, pub's keys must be sorted in lexycographical order.
	// Libfilezilla's json library already does this by default, since it uses a std::map internally.
	// FIXME: is this going to be guaranteed?
	// SEE: https://datatracker.ietf.org/doc/html/rfc7638#section-3.2
	hash_accumulator hash(hash_algorithm::sha256);
	hash.update(jwk.second.to_string());
	return fz::base64_encode(hash.digest(), base64_type::url, false);
}

/**************************************************************/

client::challenger::external::external(const serve_challenges::externally &how)
	: how_(how)
{}

client::challenger::external::~external()
{
	for (auto &p: paths_)
		fz::remove_file(p, false);
}

native_string client::challenger::external::serve(const std::string &token, const std::pair<json, json> &jwk)
{
	if (!how_.well_known_path.is_absolute())
		return fz::sprintf(fzT("Invalid well known path: %s"), how_.well_known_path.str());

	if (how_.create_if_not_existing) {
		if (!fz::mkdir(how_.well_known_path, true))
			return fz::sprintf(fzT("Couldn't create well known path: %s"), how_.well_known_path.str());
	}

	const auto &token_path = how_.well_known_path / fz::to_native(token);

	fz::file out;

	if (!out.open(token_path, fz::file::writing, fz::file::empty))
		return fz::sprintf(fzT("Couldn't open token file '%s' for writing"), token_path.str());

	paths_.push_back(token_path);

	if (!util::io::write(out, token + "." + get_jwk_fingerprint(jwk)))
		return fz::sprintf(fzT("Couldn't write token to file '%s'"), token_path.str());

	return {};
}

/**************************************************************/

client::challenger::internal::internal(thread_pool &pool, event_loop &loop, logger_interface &logger, const serve_challenges::internally &how)
	: logger_(logger, "Internal challenger")
	, how_(how)
	, context_(pool, loop)
	, server_(context_, logger, *this)
{
	server_.set_listen_address_infos(how_.addresses_info.begin(), how_.addresses_info.end());
	server_.start();
}

native_string client::challenger::internal::serve(const std::string &token, const std::pair<json, json> &jwk)
{
	fz::scoped_lock lock(mutex_);

	map_[token] = get_jwk_fingerprint(jwk);
	server_.start();

	return {};
}

class client::challenger::internal::session
	: public tcp::session
	, public event_handler
	, private buffer_operator::streamed_adder
	, private http::message_consumer
{
public:
	session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, internal &owner);

	~session() override;

private:
	int process_message_start_line(std::string_view line) override;
	int process_end_of_message_headers() override;

	bool is_alive() const override;
	void shutdown(int err = 0) override;

	void operator()(const event_base &ev) override;

private:
	internal &owner_;
	decltype(internal::map_)::const_iterator challenge_it_ = owner_.map_.end();
	std::unique_ptr<socket> socket_;
	channel channel_;
	util::invoker_handler invoke_later_;
};

std::unique_ptr<tcp::session> client::challenger::internal::make_session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &, int &error)
{
	if (!socket || error)
		return {};

	return std::make_unique<session>(
		target_handler,
		id,
		std::move(socket),
		*this
	);
}

client::challenger::internal::session::session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, internal &owner)
	: tcp::session(target_handler, id, {socket->peer_ip(), socket->address_family()})
	, event_handler(target_handler.event_loop_)
	, streamed_adder()
	, http::message_consumer(owner.logger_, 4096)
	, owner_(owner)
	, socket_(std::move(socket))
	, channel_(*this, 4096, 5, false)
	, invoke_later_(event_loop_)
{
	owner_.logger_.log_u(logmsg::debug_debug, L"Session created, with ID %d", id);

	invoke_later_([this] {
		channel_.set_buffer_adder(this);
		channel_.set_buffer_consumer(this);
		channel_.set_socket(socket_.get());
	});
}

client::challenger::internal::session::~session()
{
	owner_.logger_.log_u(logmsg::debug_debug, L"Session destroyed, with ID %d", get_id());
	remove_handler();
}

int client::challenger::internal::session::process_message_start_line(std::string_view line)
{
	static const std::string_view well_known_path = "/.well-known/acme-challenge/";
	static const std::string_view http_ver = "HTTP/1.";

	auto parts = fz::strtok_view(line, " ", false);

	auto ok = parts.size() == 3;
	ok = ok && parts[0] == "GET";
	ok = ok && fz::starts_with(parts[1], well_known_path);
	ok = ok && fz::starts_with(parts[2], http_ver);
	ok = ok && parts[2].size() == http_ver.size()+1;

	if (ok) {
		auto token = parts[1].substr(well_known_path.size());

		owner_.logger_.log_u(logmsg::debug_verbose, L"Token requested: [%s]", token);

		fz::scoped_lock lock(owner_.mutex_);

		if (challenge_it_ = owner_.map_.find(std::string(token)); challenge_it_ != owner_.map_.end())
			return 0;

		owner_.logger_.log_u(logmsg::error, L"Token has NOT been found. Reporting error.");
	}
	else {
		owner_.logger_.log_u(logmsg::error, L"Request is malformed. Reporting error.");
	}

	buffer_stream()
		<< "HTTP/1.1 400 Bad Request\r\n"
		<< "Connection: close\r\n";

	channel_.shutdown(0);
	return 0;
}

int client::challenger::internal::session::process_end_of_message_headers()
{
	fz::scoped_lock lock(owner_.mutex_);

	if (challenge_it_ == owner_.map_.end())
		return 0;

	owner_.logger_.log_u(logmsg::debug_verbose, L"Token has been found [%s]. Responding to the challenge.", challenge_it_->first);

	buffer_stream()
		<< "HTTP/1.1 200 OK\r\n"
		   "Connection: close\r\n"
		   "Content-Type: application/octet-stream\r\n"
		   "Content-Length: " << challenge_it_->first.size() + 1 + challenge_it_->second.size() << "\r\n"
		   "\r\n"
		<< challenge_it_->first << "." << challenge_it_->second;

	channel_.shutdown();
	return 0;
}

bool client::challenger::internal::session::is_alive() const
{
	return socket_ != nullptr;
}

void client::challenger::internal::session::shutdown(int err)
{
	channel_.shutdown(err);
}

void client::challenger::internal::session::operator()(const event_base &ev)
{
	fz::dispatch<channel::done_event>(ev, [this](channel &, channel::error_type error){
		target_handler_.send_event<ended_event>(id_, error);
	});
}


}
