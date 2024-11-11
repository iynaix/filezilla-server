#include <clocale>

#include "../../filezilla/serialization/archives/xml.hpp"
#include "../../server/server_config_paths.hpp"

#include "../../server/server_settings.hpp"
#include "../../filezilla/authentication/file_based_authenticator.hpp"
#include "../../filezilla/service.hpp"
#include "../../filezilla/tls_exit.hpp"
#include "../../filezilla/logger/file.hpp"

#include "server_config.hpp"
#include "converter.hpp"

int FZ_SERVICE_PROGRAM_MAIN(argc, argv)
{
	auto start = [] (int argc, char *argv[]) -> int {
		std::setlocale(LC_ALL, "");
		fz::logger::file logger(fz::logger::file::options()
			.include_headers(false)
			.start_line(false)
			.short_type_tag(false)
		);

		if (argc < 3) {
			logger.log_u(fz::logmsg::error, "Wrong number of arguments.\nUsage: %s \"path/to/FileZilla Server.xml\" [new %s service name]", argv[0], fz::build_info::package_name);

			return EINVAL;
		}

		auto old_config_path = fz::to_native(argv[1]);
		auto service_name = fz::to_native(argv[2]);

		using namespace fz::serialization;

		fz::configuration::old::server_config old_config;

		// Load up old configuration file
		{
			xml_input_archive::file_loader old_config_loader(old_config_path);
			xml_input_archive ar(old_config_loader, xml_input_archive::options().root_node_name("FileZillaServer"));

			auto &err = ar(nvp(old_config, "")).error();
			if (err) {
				logger.log_u(fz::logmsg::error, L"%s", err.description().c_str());
				return err;
			}
		}

		// Convert Old -> New
		server_settings settings;
		fz::authentication::file_based_authenticator::groups groups;
		fz::authentication::file_based_authenticator::users users;
		fz::tcp::binary_address_list disallowed_ips;
		fz::tcp::binary_address_list allowed_ips;

		fz::authentication::file_based_authenticator::groups::value_type *speed_limited_group{};

		fz::configuration::old::converter converter(old_config, logger);

		bool success =
			converter.extract(groups, &speed_limited_group) &&
			converter.extract(users, speed_limited_group) &&
			converter.extract(settings) &&
			converter.extract(disallowed_ips, allowed_ips);

		// Save new config files
		if (success) {
			auto save = [](fz::native_string path, const char *name, auto && value) -> bool {
				xml_output_archive::file_saver saver(path);
				xml_output_archive ar(saver);

				return ar(nvp(value, name)).error() == 0;
			};

			server_config_paths config_paths(service_name);

			bool success = true;

			success = success && fz::authentication::file_based_authenticator::save(
				config_paths.groups(fz::file::writing), groups,
				config_paths.users(fz::file::writing), users
			);

			success = success &&
				save(config_paths.settings(fz::file::writing), "", settings) &&
				save(config_paths.disallowed_ips(fz::file::writing), "disallowed_ips", disallowed_ips) &&
				save(config_paths.allowed_ips(fz::file::writing), "allowed_ips", allowed_ips);

			if (!success) {
				logger.log_u(fz::logmsg::error, L"failed saving configuration files.");
				return EIO;
			}

			return EXIT_SUCCESS;
		}

		return EXIT_FAILURE;
	};

	fz::tls_exit(fz::service::make(argc, argv, start));
}
