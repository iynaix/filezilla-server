#include "server.hpp"
#include "server/session.hpp"
#include "server/request.hpp"

namespace fz::http {

server::server(tcp::server::context &context, event_loop_pool &event_loop_pool, transaction_handler &transaction_handler,
	tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, logger_interface &logger
)
	: tcp::server::delegate<server>(tcp_server_)
	, tcp::session::factory::base(event_loop_pool, disallowed_ips, allowed_ips, autobanner, logger, "HTTP Server")
	, transaction_handler_(transaction_handler)
	, logger_(logger, "HTTP Server")
	, tcp_server_(context, logger_, *this)
{
}

void server::set_security_info(const securable_socket::info &info)
{
	fz::scoped_lock lock(mutex_);
	security_info_ = info;
}

void server::set_timeouts(const duration &keepalive_timeout, const duration &activity_timeout)
{
	iterate_over_sessions({}, [&keepalive_timeout, &activity_timeout](session &s) {
		s.set_timeouts(keepalive_timeout, activity_timeout);
		return true;
	});

	fz::scoped_lock lock(mutex_);
	keepalive_timeout_ = keepalive_timeout;
	activity_timeout_ = activity_timeout;
}

std::unique_ptr<tcp::session> server::make_session(event_handler &target_handler, event_loop &loop, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error)
{
	auto use_tls = std::any_cast<bool>(&user_data);
	if (!use_tls) {
		// This should really never ever happen
		logger_.log(logmsg::error, L"User data is not of the proper type. This is an internal error.");
		error = EINVAL;
	}

	std::unique_ptr<session> p;

	if (socket && !error) {
		scoped_lock lock(mutex_);

		p = std::make_unique<session>(
			target_handler,
			loop,
			id,
			std::move(socket),
			*use_tls ? &security_info_ : nullptr,
			transaction_handler_,
			logger_
		);

		p->set_timeouts(keepalive_timeout_, activity_timeout_);
	}

	return p;
}

}
