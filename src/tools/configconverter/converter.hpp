#ifndef FZ_CONFIGURATION_OLD_CONVERTER_HPP
#define FZ_CONFIGURATION_OLD_CONVERTER_HPP

#include <libfilezilla/logger.hpp>

#include "../../server/server_settings.hpp"
#include "../../filezilla/authentication/file_based_authenticator.hpp"

namespace fz::configuration::old {

struct server_config;

class converter {
public:
	converter(server_config &config, logger_interface &logger = fz::get_null_logger())
		: config_(config)
		, logger_(logger)
	{}

	bool extract(fz::authentication::file_based_authenticator::groups &groups, fz::authentication::file_based_authenticator::groups::value_type **speed_limited_group = nullptr);
	bool extract(fz::authentication::file_based_authenticator::users &users, fz::authentication::file_based_authenticator::groups::value_type *speed_limited_group = nullptr);
	bool extract(server_settings& settings);
	bool extract(fz::tcp::binary_address_list &disallowed_ips, fz::tcp::binary_address_list &allowed_ips);

private:
	server_config &config_;
	logger_interface &logger_;
};

}

#endif // FZ_CONFIGURATION_OLD_CONVERTER_HPP
