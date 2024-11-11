#include "server_settings.hpp"

bool server_settings::convert_legacy(fz::logger_interface &logger)
{
	if (!legacy_ftp_server)
		return false;

	auto &l = *legacy_ftp_server;

	protocols.autobanner.ban_duration(l.brute_force_protection_duration());
	protocols.autobanner.login_failures_time_window(l.sessions().login_attempts_failure_tolerance_duration);
	protocols.autobanner.max_login_failures(l.sessions().number_of_allowed_failed_login_attempts);

	protocols.performance.number_of_session_threads = l.number_of_session_threads();
	protocols.performance.receive_buffer_size = l.sessions().receive_buffer_size;
	protocols.performance.send_buffer_size = l.sessions().send_buffer_size;

	protocols.timeouts.activity_timeout = l.sessions().activity_timeout;
	protocols.timeouts.login_timeout = l.sessions().login_timeout;

	ftp_server.listeners_info(l.control_info_list());
	ftp_server.sessions().pasv.do_not_override_host_if_peer_is_local = l.sessions().pasv.do_not_override_host_if_peer_is_local;
	ftp_server.sessions().pasv.host_override = l.sessions().pasv.host_override;

	if (auto &p = l.sessions().pasv.port_range)
		ftp_server.sessions().pasv.port_range = { p->min, p->max };

	ftp_server.welcome_message(l.welcome_message());

	logger.log_raw(fz::logmsg::status, L"Converted legacy settings.");

	return true;
}
