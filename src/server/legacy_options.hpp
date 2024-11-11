// This file is here only for backwards compatibility. The structures and functions defined here are not to be used for new code.

#ifndef LEGACY_OPTIONS_HPP
#define LEGACY_OPTIONS_HPP

#include <vector>
#include <libfilezilla/string.hpp>
#include <libfilezilla/time.hpp>

#include "../filezilla/util/options.hpp"
#include "../filezilla/tcp/address_info.hpp"
#include "../filezilla/port_randomizer.hpp"
#include "../filezilla/securable_socket.hpp"
#include "../filezilla/ftp/server.hpp"

namespace fz::legacy {

namespace ftp {

struct session {
	struct options {
		struct pasv {
			struct port_range {
				std::uint16_t  min = port_randomizer::min_ephemeral_value;
				std::uint16_t  max = port_randomizer::max_ephemeral_value;

				port_range() {}
			};

			fz::native_string host_override;
			bool do_not_override_host_if_peer_is_local{true};
			std::optional<port_range> port_range;
		};

		std::uint16_t          number_of_allowed_failed_login_attempts   = 0;
		fz::duration           login_attempts_failure_tolerance_duration = duration::from_milliseconds(100);
		fz::duration           login_timeout                             = duration::from_minutes(1);
		fz::duration           activity_timeout                          = duration::from_hours(1);
		pasv                   pasv                                      = {};
		securable_socket::info tls                                       = {};

		std::int32_t receive_buffer_size = -1;
		std::int32_t send_buffer_size    = -1;

		options(){}
	};
};

struct server {
	struct options: util::options<options> {
		opt<std::vector<fz::ftp::address_info>> control_info_list               = o();
		opt<fz::duration>                       brute_force_protection_duration = o(duration::from_minutes(5));
		opt<std::uint16_t>                      number_of_session_threads       = o();
		opt<session::options>                   sessions                        = o();
		opt<std::string>                        welcome_message                 = o();

		options(){}
	};
};

template <typename Archive>
void serialize(Archive &ar, struct ftp::session::options::pasv::port_range &o)
{
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.min,
				   "min"),
				   "Maximum value for the port range to be used for PASV connections"),

		value_info(optional_nvp(o.max,
				   "max"),
				   "Maximum value for the port range to be used for PASV connections")
	);
}

template <typename Archive>
void serialize(Archive &ar, struct ftp::session::options::pasv &o)
{
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.host_override,
				   "host_override"),
				   "IPV4 IP or name host that overrides the local address when PASV is used. Leave empty to not perform the override"),

		value_info(optional_nvp(o.do_not_override_host_if_peer_is_local,
				   "do_not_override_host_if_peer_is_local"),
				   "If set to true, then the host is not overriden for local connections."),

		value_info(nvp(o.port_range,
				   "port_range"),
				   "Port range to be used for PASV connections")
	);
}

template <typename Archive>
void serialize(Archive &ar, struct ftp::session::options &o)
{
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.number_of_allowed_failed_login_attempts,
				   "number_of_allowed_failed_login_attempts"),
				   "The number of login attempts that are allowed to fail, within the timeframe specified by the parameter [login_attempts_failure_tolerance_duration]. "
				   "The value 0 disables this mechanism."),

		value_info(optional_nvp(o.login_attempts_failure_tolerance_duration,
				   "login_attempts_failure_tolerance_duration"),
				   "The duration, in millisecond, during which the number of failed login attempts is monitored."),

		value_info(optional_nvp(o.login_timeout,
				   "login_timeout"),
				   "Login timeout (fz::duration)"),

		value_info(optional_nvp(o.activity_timeout,
				   "activity_timeout"),
				   "Activity timeout (fz::duration)."),

		value_info(optional_nvp(o.pasv,
				   "pasv"),
				   "PASV settings"),

		value_info(optional_nvp(o.tls,
				   "tls"),
				   "TLS certificate data."),

		value_info(optional_nvp(o.receive_buffer_size,
				   "receive_buffer_size"),
				   "Size of receving data socket buffer. Numbers < 0 mean use system defaults. Defaults to -1."),

		value_info(optional_nvp(o.send_buffer_size,
				   "send_buffer_size"),
				   "Size of sending data socket buffer. Numbers < 0 mean use system defaults. Defaults to -1.")
	);
}

template <typename Archive>
void serialize(Archive &ar, fz::legacy::ftp::server::options &o)
{
	using namespace serialization;

	ar(
		value_info(nvp(o.control_info_list(), "",
				  "listener"),
				  "List of addresses and ports the FTP server will listen on."),

		value_info(optional_nvp(o.brute_force_protection_duration(),
				   "brute_force_protection_duration"),
				   "The duration, in millisecond, during which a given IP is put in black list, as a brute force protection measure."),

		value_info(optional_nvp(o.number_of_session_threads(),
				   "number_of_session_threads"),
				   "Number of threads to distribute sessions to."),

		value_info(optional_nvp(o.sessions(),
				   "session"),
				   "Session-related options."),

		value_info(optional_nvp(o.welcome_message(),
				   "welcome_message"),
				   "Additional welcome message.")
	);
}

}

}

#endif // LEGACY_OPTIONS_HPP
