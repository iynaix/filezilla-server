#ifndef SERVER_SETTINGS_HPP
#define SERVER_SETTINGS_HPP

#include <libfilezilla/time.hpp>
#include <libfilezilla/event_handler.hpp>

#include "../filezilla/logger/null.hpp"
#include "../filezilla/tcp/address_info.hpp"
#include "../filezilla/authentication/password.hpp"

#include "../filezilla/serialization/types/ftp_server_options.hpp"
#include "../filezilla/serialization/types/logger_file_options.hpp"
#include "../filezilla/serialization/types/securable_socket_cert_info.hpp"
#include "../filezilla/serialization/types/time.hpp"
#include "../filezilla/serialization/types/variant.hpp"
#include "../filezilla/serialization/types/containers.hpp"
#include "../filezilla/serialization/types/acme.hpp"
#include "../filezilla/serialization/types/autobanner.hpp"
#include "../filezilla/serialization/types/update.hpp"
#include "../filezilla/rmp/address_info.hpp"
#include "../filezilla/serialization/types/webui_server_options.hpp"

#include "../filezilla/acme/challenges.hpp"
#include "legacy_options.hpp"

#include "server_config_paths.hpp"

struct server_settings {
	std::string locale               = "";
	fz::logger::file::options logger = {};

	struct admin_options {
		admin_options() {}

		unsigned int                       local_port = 14148;
		bool                               enable_local_ipv6 = true;
		std::vector<fz::rmp::address_info> additional_address_info_list;
		fz::authentication::any_password   password = {};
		fz::securable_socket::info         tls = {};

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar(
				optional_nvp(local_port, "local_port"),
				optional_nvp(enable_local_ipv6, "enable_local_ipv6"),
				nvp(unique(additional_address_info_list), "", "listener"),
				optional_nvp(password, "password"),
				optional_nvp(tls, "tls")
			);
		}
	};

	fz::webui::server::options webui;

	admin_options admin;

	struct protocols_options {
		struct performance_options {
			std::uint16_t number_of_session_threads = 0;
			std::int32_t receive_buffer_size        = -1;
			std::int32_t send_buffer_size           = -1;

			template <typename Archive>
			void serialize(Archive &ar) {
				using namespace fz::serialization;

				ar(
					value_info(optional_nvp(number_of_session_threads,
						"number_of_session_threads"),
						"Number of threads to distribute sessions to."),

					value_info(optional_nvp(receive_buffer_size,
							   "receive_buffer_size"),
							   "Size of receving data socket buffer. Numbers < 0 mean use system defaults. Defaults to -1."),

					value_info(optional_nvp(send_buffer_size,
							   "send_buffer_size"),
							   "Size of sending data socket buffer. Numbers < 0 mean use system defaults. Defaults to -1.")
				);
			}
		};

		struct timeout_options {
			fz::duration login_timeout    = fz::duration::from_minutes(1);
			fz::duration activity_timeout = fz::duration::from_hours(1);

			template <typename Archive>
			void serialize(Archive &ar) {
				using namespace fz::serialization;

				ar(
					value_info(optional_nvp(login_timeout,
							   "login_timeout"),
							   "Login timeout (fz::duration)"),

					value_info(optional_nvp(activity_timeout,
							   "activity_timeout"),
							   "Activity timeout (fz::duration).")
				);
			}
		};

		fz::authentication::autobanner::options autobanner = {};
		performance_options performance = {};
		timeout_options timeouts = {};

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar(
				value_info(optional_nvp(autobanner,
						   "autobanner"),
						   "Autobanner options"),

				value_info(optional_nvp(performance,
					"performance"),
					"Performance options."),

				value_info(optional_nvp(timeouts,
					"timeouts"),
					"Timeout options.")
			);
		}
	};

	protocols_options protocols;

	std::optional<fz::legacy::ftp::server::options> legacy_ftp_server = {};

	fz::ftp::server::options ftp_server = {};

	struct acme_options {
		acme_options() {}

		std::string account_id;
		fz::acme::serve_challenges::how how_to_serve_challenges;

		explicit operator bool() const
		{
			return !account_id.empty() && how_to_serve_challenges;
		}

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar(
				optional_nvp(account_id, "account_id"),
				optional_nvp(how_to_serve_challenges, "how_to_serve_challenges")
			);
		}
	};

	acme_options acme;

	fz::update::checker::options update_checker = {};

	struct pkcs11_options {
		pkcs11_options() {}

		std::vector<fz::native_string> providers;

		bool operator==(const pkcs11_options &rhs) const
		{
			return providers == rhs.providers;
		}

		bool operator!=(const pkcs11_options &rhs) const
		{
			return !(*this == rhs);
		}

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar(
				value_info(optional_nvp(providers,
						   "", "provider"),
						   "The list of PKCS#11 providers to be loaded. If empty, the default system config will be used.")
			);
		}
	};

	pkcs11_options pkcs11;

	template <typename Archive>
	void serialize(Archive &ar) {
		using namespace fz::serialization;

		ar(
			value_info(optional_nvp(locale,
					   "locale"),
					   "The server's locale. By default, the one defined by the appropriate environment variables is used."),

			value_info(optional_nvp(logger,
					   "logger"),
					   "Logging options."),

			value_info(optional_nvp(protocols,
					   "all_protocols"),
					   "Settings common to all file transfer protocols"),

			value_info(optional_nvp(ftp_server,
					   "ftp_server"),
					   "FTP Server options"),

			value_info(optional_nvp(webui,
					   "webui"),
					   "WebUI options"),

			value_info(optional_nvp(admin,
					   "admin"),
					   "Administration options."),

			value_info(optional_nvp(acme,
					   "acme"),
					   "ACME (Let's Encrypt® and the like) settings."),

			value_info(optional_nvp(update_checker,
					   "update_checker"),
					   "Update checker options."),

			value_info(optional_nvp(pkcs11,
					   "pkcs11"),
					   "PKCS#11 options.")

		);

		if constexpr (trait::is_input_v<Archive>)
			ar(nvp(legacy_ftp_server, "server"));
	}

	bool convert_legacy(fz::logger_interface &logger = fz::logger::null);
};

#endif // SETTINGS_HPP
