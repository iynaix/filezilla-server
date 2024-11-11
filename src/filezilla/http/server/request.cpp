#include "request.hpp"
#include "transaction.hpp"
#include "session.hpp"

namespace fz::http {

tcp::session::id server::request::get_session_id() const
{
	if (auto s = t_.get_session()) {
		return s->get_id();
	}

	return {};
}

std::string server::request::get_peer_address() const
{
	if (auto s = t_.get_session()) {
		return s->get_peer_info().first;
	}

	return {};
}

fz::address_type server::request::get_peer_address_type() const
{
	if (auto s = t_.get_session()) {
		return s->get_peer_info().second;
	}

	return {};
}

bool server::request::is_secure() const
{
	if (auto s = t_.get_session()) {
		return s->is_secure();
	}

	return {};
}

server::request::request(transaction &t)
	: t_(t)
{
}

server::request::~request()
{
	headers.clear();
}

void server::request::receive_body(std::string &&body, std::function<void (std::string, bool)> on_end)
{
	if (auto s = t_.get_session()) {
		s->receive_body({}, std::move(body), std::move(on_end));
	}
	else {
		on_end(std::move(body), false);
	}
}

void server::request::receive_body(tvfs::file_holder &&file, std::function<void (tvfs::file_holder, bool)> on_end)
{
	if (auto s = t_.get_session()) {
		s->receive_body({}, std::move(file), std::move(on_end));
	}
	else {
		on_end(std::move(file), false);
	}
}

}
