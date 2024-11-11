#include <libfilezilla/util.hpp>

#include <cstring>

#include "session.hpp"
#include "server.hpp"
#include "../authentication/autobanner.hpp"

#include "../tcp/address_list.hpp"

namespace fz::ftp {

server::server(tcp::server::context &context, event_loop_pool &loop_pool,
			   logger_interface &nonsession_logger, logger_interface &session_logger,
			   authentication::authenticator &authenticator,
			   rate_limit_manager &rate_limit_manager,
			   tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips,
			   authentication::autobanner &autobanner,
			   port_manager &port_manager,
			   options opts)
	: tcp::server::delegate<server>(tcp_server_)
	, event_handler(context.loop())
	, tcp::session::factory::base(loop_pool, disallowed_ips, allowed_ips, autobanner, nonsession_logger, "FTP Server")
	, pool_(context.pool())
	, nonsession_logger_(nonsession_logger, "FTP Server")
	, session_logger_(session_logger, "FTP Server")
	, authenticator_(authenticator)
	, rate_limit_manager_(rate_limit_manager)
	, autobanner_(autobanner, *this)
	, port_manager_(port_manager)
	, tcp_server_(context, nonsession_logger_, *this)
{
	set_options(std::move(opts));
}

server::~server()
{
	remove_handler();
}

void server::refuse_connections(std::string_view refuse_message)
{
	fz::scoped_lock lock(mutex_);
	refuse_message_ = refuse_message;
}

void server::set_options(server::options opts)
{
	set_listen_address_infos(opts.listeners_info().begin(), opts.listeners_info().end());

	if (opts.listeners_info().empty())
		nonsession_logger_.log_u(fz::logmsg::debug_warning, L"No listeners were set. Will not serve!");

	if (auto res = opts.welcome_message().validate(); !res) {
		nonsession_logger_.log_u(fz::logmsg::error, L"Welcome message is invalid: %s. Ignoring.",
			res == res.total_size_too_big ? "total size is too big." :
			res == res.line_too_long ? "some lines are too long." :
			"unknown reason");
		opts.welcome_message() = {};
	}

	iterate_over_sessions({}, [&opts](fz::ftp::session &s) {
		s.set_options(opts.sessions());
		return true;
	});

	fz::scoped_lock lock(mutex_);
	opts_ = std::move(opts);
}

void server::set_data_buffer_sizes(int32_t receive, int32_t send)
{
	iterate_over_sessions({}, [&receive, &send](fz::ftp::session &s) {
		s.set_data_buffer_sizes(receive, send);
		return true;
	});

	fz::scoped_lock lock(mutex_);
	receive_buffer_size_ = receive;
	send_buffer_size_    = send;
}

void server::set_timeouts(const duration &login_timeout, const duration &activity_timeout)
{
	iterate_over_sessions({}, [&login_timeout, &activity_timeout](fz::ftp::session &s) {
		s.set_timeouts(login_timeout, activity_timeout);
		return true;
	});

	fz::scoped_lock lock(mutex_);
	login_timeout_ = login_timeout;
	activity_timeout_ = activity_timeout;
}

void server::set_notifier_factory(session::notifier::factory &nf)
{
	scoped_lock lock(mutex_);

	notifier_factory_ = &nf;
}

std::unique_ptr<tcp::session> server::make_session(event_handler &target_handler, event_loop &loop, tcp::session::id session_id, std::unique_ptr<socket> socket, const std::any &user_data, int &error)
{
	auto tls_mode = std::any_cast<session::tls_mode>(&user_data);
	if (!tls_mode) {
		// This should really never ever happen
		session_logger_.log(logmsg::error, L"User data is not of the proper type. This is an internal error.");
		error = EINVAL;
	}

	if (!socket || error)
		return {};

	socket->set_flags(socket::flag_keepalive | socket::flag_nodelay);
	socket->set_keepalive_interval(duration::from_seconds(30));

	scoped_lock lock(mutex_);

	auto startdate = datetime::now();
	auto notifier = (*notifier_factory_).make_notifier(session_id, startdate, socket->peer_ip(), socket->address_family(), session_logger_);

	auto session = std::make_unique<ftp::session>(
		pool_,
		loop,
		target_handler,
		rate_limit_manager_,
		std::move(notifier),
		session_id,
		startdate,
		std::move(socket),
		*tls_mode,
		autobanner_,
		authenticator_,
		port_manager_,
		opts_.welcome_message(),
		refuse_message_,
		opts_.sessions()
	);

	session->set_data_buffer_sizes(receive_buffer_size_, send_buffer_size_);
	session->set_timeouts(login_timeout_, activity_timeout_);

	return session;
}

void server::listener_status_changed(const tcp::listener &listener)
{
	notifier_factory_->listener_status(listener);
}

bool server::log_on_session_exit()
{
	return false;
}

void server::operator()(const event_base &ev)
{
	fz::dispatch<authentication::autobanner::banned_event>(ev, this,
		&server::on_banned_event
	);
}

void server::on_banned_event(const std::string &address, address_type type)
{
	iterate_over_sessions({}, [&](fz::ftp::session &s) {
		if (s.get_peer_info() == std::pair{address, type})
			s.shutdown();

		return true;
	});
}

}
