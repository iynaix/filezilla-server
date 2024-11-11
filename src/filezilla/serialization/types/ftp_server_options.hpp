#ifndef FZ_SERIALIZATION_TYPES_FTP_SERVER_OPTIONS_HPP
#define FZ_SERIALIZATION_TYPES_FTP_SERVER_OPTIONS_HPP

#include "../../serialization/types/optional.hpp"
#include "../../serialization/types/containers.hpp"

#include "../../ftp/server.hpp"

namespace fz::serialization {

template <typename Archive>
void serialize(Archive &ar, struct ftp::session::options::pasv::port_range &o)
{
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.min,
				   "min"),
				   "Minimum value for the port range to be used for PASV connections"),

		value_info(optional_nvp(o.max,
				   "max"),
				   "Maximum value for the port range to be used for PASV connections")
	);

	if constexpr (trait::is_input_v<Archive>) {
		if (o.max < o.min)
			std::swap(o.max, o.min);
	}
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
		value_info(optional_nvp(o.pasv,
				   "pasv"),
				   "PASV settings"),

		value_info(optional_nvp(o.tls,
				   "tls"),
				   "TLS certificate data.")
	);
}

template <typename Archive>
void serialize(Archive &ar, ftp::server::options &o)
{
	using namespace serialization;

	ar(
		value_info(nvp(unique(o.listeners_info()), "",
				  "listener"),
				  "List of addresses and ports the FTP server will listen on."),

		value_info(optional_nvp(o.sessions(),
				   "session"),
				   "Session-related options."),

		value_info(optional_nvp(o.welcome_message(),
				   "welcome_message"),
				   "Additional welcome message.")
	);
}

template <typename Archive>
void serialize(Archive &ar, ftp::commander::welcome_message &w)
{
	using namespace serialization;

	ar(
		nvp(static_cast<std::string&>(w), ""),
		optional_attribute(w.has_version, "has_version")
	);
}

template <typename Archive>
void serialize(Archive &ar, ftp::session::protocol_info &i)
{
	using namespace serialization;

	ar(
		nvp(i.security, "security")
	);
}

template <typename Archive>
void serialize(Archive &ar, ftp::session::protocol_info::security_t::value_type &s)
{
	using namespace serialization;

	ar(
		nvp(s.cipher, "cipher"),
		nvp(s.mac, "mac"),
		nvp(s.tls_version, "tls_version"),
		nvp(s.key_exchange, "key_exchange"),
		nvp(s.algorithm_warnings, "algorithm_warnings")
	);
}

}

FZ_SERIALIZATION_SPECIALIZE_ALL(ftp::commander::welcome_message, unversioned_nonmember_serialize)

#endif // FZ_SERIALIZATION_TYPES_FTP_SERVER_OPTIONS_HPP
