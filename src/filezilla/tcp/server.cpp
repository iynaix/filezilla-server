#include <libfilezilla/util.hpp>

#include "../tcp/server.hpp"

namespace fz::tcp {

server::server(server::context &context, logger_interface &logger, session::factory &session_factory)
	: event_handler(context.loop())
	, context_(context)
	, logger_(logger)
	, session_factory_(session_factory)
	, listeners_loop_(context.pool())
	, listeners_(context.pool(), listeners_loop_, *this, logger_, session_factory_, session_factory_)
{
}

server::~server()
{
	logger_.log_u(logmsg::debug_debug, L"Destroying.");
	remove_handler();
	stop(true);
}

std::size_t server::end_sessions(const std::vector<session::id> &ids, int err)
{
	scoped_lock lock(mutex_);

	std::size_t num_ended = 0;

	// An empty list of peers ids means: disconnect all peers
	if (ids.empty()) {
		for (auto &s: sessions_) {
			++num_ended;
			s.second->shutdown(err);
		}
	}
	else {
		for (auto id: ids) {
			if (auto it = sessions_.find(id); it != sessions_.end()) {
				++num_ended;
				it->second->shutdown(err);
			}
		}
	}

	return num_ended;
}

util::locked_proxy<session> server::get_session(session::id id)
{
	mutex_.lock();

	session *s = [&]() -> session * {
		auto it = sessions_.find(id);
		if (it != sessions_.end())
			return it->second.get();

		return nullptr;
	}();

	return {s, &mutex_};
}

bool server::start()
{
	return listeners_.start();
}

bool server::stop(bool destroy_all_sessions)
{
	if (!listeners_.stop()) {
		return false;
	}

	// Session destruction must happen outside mutex, see also on_session_ended_event
	decltype(sessions_) sessions_to_destroy;

	scoped_lock lock(mutex_);

	if (destroy_all_sessions) {
		logger_.log_u(logmsg::debug_debug, L"Destroying sessions.");
		sessions_to_destroy = std::move(sessions_);
	}

	return true;
}

bool server::is_running() const
{
	return listeners_.is_running();
}

void server::operator ()(const event_base &ev)
{
	fz::dispatch<
		tcp::listener::connected_event,
		typename session::ended_event
	>(ev, this,
		&server::on_connected_event,
		&server::on_session_ended_event
	);
}

void server::on_connected_event(tcp::listener &listener)
{
	std::size_t loop_count = 10;

	while (auto socket = listener.get_socket()) {
		int error = 0;
		auto id = context_.next_session_id();
		auto session = session_factory_.make_session(*this, id, std::move(socket), listener.get_user_data(), error);

		if (session) {
			scoped_lock lock(mutex_);
			sessions_.insert({id, std::move(session)});
			num_sessions_ += 1;
		}

		// Don't starve the event loop
		if (--loop_count == 0) {
			if (listener.has_socket()) {
				send_event<listener::connected_event>(listener);
				break;
			}
		}
	}
}

void server::on_session_ended_event(session::id id, const channel::error_type &error)
{
	decltype(sessions_)::node_type extracted;

	{
		scoped_lock lock(mutex_);
		num_sessions_ -= 1;
		extracted = sessions_.extract(id);
	}

	if (!extracted)
		return;

	if (!error)
		logger_.log_u(logmsg::status, "Session %d ended gracefully.", id);
	else
	if (session_factory_.log_on_session_exit() && error != ECONNRESET)
		logger_.log_u(logmsg::error, "Session %d ended with error. Reason: %s.", id, socket_error_description(error.error()));
}

}
