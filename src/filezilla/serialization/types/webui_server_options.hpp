#ifndef FZ_SERIALIZATION_TYPES_WEBUI_SERVER_OPTIONS_HPP
#define FZ_SERIALIZATION_TYPES_WEBUI_SERVER_OPTIONS_HPP

#include "../../webui/server.hpp"
#include "../../serialization/types/containers.hpp"

namespace fz::serialization
{

template <typename Archive>
void serialize(Archive &ar, webui::server::options &o)
{
	using namespace ::fz::serialization;

	ar(
		value_info(optional_nvp(o.access_token_timeout,
								"access_timeout"),
								"Expiration timeout for the access token"),

		value_info(optional_nvp(o.refresh_token_timeout,
								"refresh_timeout"),
								"Expiration timeout for the refresh token"),
		value_info(optional_nvp(o.http_keepalive_timeout,
								"http_keepalive_timeout"),
								"HTTP keepalive timeout"),

		value_info(optional_nvp(o.http_activity_timeout,
								"http_activity_timeout"),
								"HTTP activity timeout"),

		value_info(nvp(unique(o.listeners_info), "",
								"listener"),
								"List of addresses and ports the FTP server will listen on."),

		value_info(optional_nvp(o.tls,
								"tls"),
								"TLS certificate cata")
	);
}

}

#endif // FZ_SERIALIZATION_TYPES_WEBUI_SERVER_OPTIONS_HPP
