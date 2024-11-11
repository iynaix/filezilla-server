#ifndef ADMINISTRATION_HPP
#define ADMINISTRATION_HPP

#include "../filezilla/tcp/session.hpp"

#include "../filezilla/serialization/types/time.hpp"
#include "../filezilla/serialization/types/binary_address_list.hpp"
#include "../filezilla/serialization/types/hostaddress.hpp"
#include "../filezilla/serialization/types/logger_file_options.hpp"
#include "../filezilla/serialization/types/ftp_server_options.hpp"
#include "../filezilla/serialization/types/autobanner.hpp"
#include "../filezilla/serialization/types/network_interface.hpp"
#include "../filezilla/serialization/types/json.hpp"
#include "../filezilla/serialization/types/update.hpp"

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "../filezilla/acme/daemon.hpp"

#include "../filezilla/rmp/engine.hpp"
#include "../filezilla/rmp/engine/visitor.hpp"
#include "../filezilla/rmp/engine/client.hpp"
#include "../filezilla/rmp/glue/receiver.hpp"

#include "../server/server_settings.hpp"

#include "../filezilla/debug.hpp"

#ifndef ENABLE_ADMIN_DEBUG
#   define ENABLE_ADMIN_DEBUG 0
#endif

#define ADMIN_LOG FZ_DEBUG_LOG("administration", ENABLE_ADMIN_DEBUG)

namespace administration {
	FZ_RMP_USING_DIRECTIVES

	// Increase this number any time a new message is added/removed/changed
	// Remember, though, that the admin_login message must come always FIRST and CANNOT be removed (but it can be changed), since it's the only one that does the version check.
	static constexpr version_t protocol_version { 61 };

	using admin_login = command <versioned<protocol_version, struct admin_login_tag> (std::string password), response(
		fz::util::fs::path_format,
		bool unsafe_ptrace_scope,
		bool can_impersonate,
		fz::native_string username,
		std::optional<std::vector<fz::network_interface>> network_interfaces,
		bool some_certificates_have_expired,
		bool any_is_equivalent,
		bool ftp_has_host_override,
		std::string server_version,
		std::string server_host,
		std::vector<uint8_t> instance_id
	)>;

	namespace session {
		struct any_protocol_info: fz::tcp::session::notifier::protocol_info, std::variant<
			fz::ftp::session::protocol_info
		>{
			using variant::variant;

			status get_status() const override
			{
				return std::visit([](auto &i){ return i.get_status(); }, static_cast<const variant &>(*this));
			}
			std::string get_name() const override
			{
				return std::visit([](auto &i){ return i.get_name(); }, static_cast<const variant &>(*this));
			}

			const fz::ftp::session::protocol_info *ftp() const
			{
				return std::get_if<fz::ftp::session::protocol_info>(static_cast<const variant *>(this));
			}
		};

		using start          = message <struct start_tag     (fz::ftp::session::id session_id, fz::datetime start_time, std::string peer_ip, fz::address_type family)>;
		using stop           = message <struct stop_tag      (fz::ftp::session::id session_id, fz::duration since_start)>;
		using user_name      = message <struct user_name_tag (fz::ftp::session::id session_id, fz::duration since_start, std::string user_name)>;

		using entry_open    = message<struct entry_open_tag  (fz::ftp::session::id session_id, fz::duration since_start, std::uint64_t entry_id, std::string path, std::int64_t size)>;
		using entry_close   = message<struct entry_close_tag (fz::ftp::session::id session_id, fz::duration since_start, std::uint64_t entry_id, int error)>;
		using entry_written = message<struct entry_write_tag (fz::ftp::session::id session_id, fz::duration since_start, std::uint64_t entry_id, std::int64_t amount, std::int64_t actual_entry_size)>;
		using entry_read    = message<struct entry_read_tag  (fz::ftp::session::id session_id, fz::duration since_start, std::uint64_t entry_id, std::int64_t amount)>;

		using protocol_info  = message <struct protocol_info_tag  (fz::ftp::session::id session_id, fz::duration since_start, any_protocol_info info)>;

		using solicit_info = message <struct solicit_info_tag (std::vector<fz::ftp::session::id> session_ids)>;
	}

	using log = message <struct log_tag (fz::datetime dt, fz::ftp::session::id session_id, fz::logmsg::type type, std::string module, std::wstring msg)>;

	using server_status   = message <struct server_status_tag (bool is_online)>;
	using listener_status = message <struct listener_status_tag (fz::datetime datetime, fz::tcp::address_info address_info, fz::tcp::listener::status status)>;

	using acknowledge_queue_full = command <struct acknowledge_queue_full_tag (), response()>;

	using ban_ip                = command <struct ban_ip_tag                     (std::string ip, fz::address_type family), response ()>;
	using end_sessions          = command <struct end_sessions_tag               (std::vector<fz::ftp::session::id> sessions), response (std::size_t num_of_ended_sessions)>;
	using set_server_status     = command <struct set_server_status_tag          (server_status status), response ()>;
	using get_groups_and_users  = command <struct get_groups_and_users_tag       (), response (fz::authentication::file_based_authenticator::groups, fz::authentication::file_based_authenticator::users)>;
	using set_groups_and_users  = command <struct set_groups_and_users_tag       (fz::authentication::file_based_authenticator::groups, fz::authentication::file_based_authenticator::users, bool do_save), response ()>;
	using get_ip_filters        = command <struct get_ip_filters_tag             (), response (fz::tcp::binary_address_list disallowed_ips, fz::tcp::binary_address_list allowed_ips)>;
	using set_ip_filters        = command <struct set_ip_filters_tag             (fz::tcp::binary_address_list disallowed_ips, fz::tcp::binary_address_list allowed_ips), response ()>;
	using set_ftp_options       = command <struct set_ftp_options_tag            (fz::ftp::server::options ftp_options), response()>;
	using get_ftp_options       = command <struct get_ftp_options_tag            (bool export_cert), response(fz::ftp::server::options ftp_options, fz::securable_socket::cert_info::extra tls_extra_certs_info)>;
	using set_protocols_options = command <struct set_protocols_options_tag      (server_settings::protocols_options), response()>;
	using get_protocols_options = command <struct get_protocols_options_tag      (), response(server_settings::protocols_options)>;
	using set_admin_options     = command <struct set_admin_options_tag          (server_settings::admin_options admin_options), response()>;
	using get_admin_options     = command <struct get_admin_options_tag          (bool export_cert), response(server_settings::admin_options admin_options, fz::securable_socket::cert_info::extra tls_extra_certs_info)>;
	using set_logger_options    = command <struct set_logger_options_tag         (fz::logger::file::options logger_options), response()>;
	using get_logger_options    = command <struct get_logger_options_tag         (), response(fz::logger::file::options logger_options)>;
	using get_acme_options      = command <struct get_acme_options_tag           (), response(server_settings::acme_options acme_options, fz::acme::extra_account_info extra)>;
	using set_acme_options      = command <struct set_acme_options_tag           (server_settings::acme_options acme_options), response()>;
	using get_pkcs11_options    = command <struct get_pkcs11_options_tag         (), response(server_settings::pkcs11_options pkcs11_options)>;
	using set_pkcs11_options    = command <struct set_pkcs11_options_tag         (server_settings::pkcs11_options pkcs11_options), response()>;
	using set_updates_options   = command <struct set_updates_options_tag        (fz::update::checker::options update_checker_options), response()>;
	using get_updates_options   = command <struct get_updates_options_tag        (), response(fz::update::checker::options update_checker_options)>;
	using set_webui_options     = command <struct set_webui_options_tag          (fz::webui::server::options webui_options), response()>;
	using get_webui_options     = command <struct get_webui_options_tag          (bool export_cert), response(fz::webui::server::options webui_options, fz::securable_socket::cert_info::extra tls_extra_certs_info)>;
	using destroy_webui_tokens  = command <struct destroy_webui_tokens_tag       (), response()>;

	using generate_selfsigned_certificate = command<struct generate_selfsigned_certificate_tag
		(std::string dinstinguished_name, std::vector<std::string> hostnames, fz::tls_param key, fz::native_string key_password),
		response(
			success(fz::securable_socket::cert_info info, fz::securable_socket::cert_info::extra extra),
			failure(fz::native_string error)
		)
	>;

	using get_extra_certs_info = command <struct get_extra_get_certs_info_tag
		(fz::securable_socket::cert_info),
		response(
			success(fz::securable_socket::cert_info::extra extra),
			failure(fz::native_string error)
		)
	>;

	using get_deobfuscated_blob = command <struct get_rmp_loop_response_string_tag
		(std::string obfuscated),
		response(std::string deobfuscated)
	>;

	using get_acme_terms_of_service = command<struct get_acme_terms_of_service_tag
		(std::string directory),
		response(
			success(std::string terms_of_service_uri),
			failure(fz::native_string error)
		)
	>;

	using generate_acme_account = command<struct generate_acme_account_tag
		(std::string directory, std::vector<std::string> contacts, bool terms_of_service_agreed),
		response(
			success(std::string account_id, fz::acme::extra_account_info extra),
			failure(fz::native_string error)
		)
	>;

	using restore_acme_account = command<struct restore_acme_account_tag
		(std::string account_id, fz::acme::extra_account_info extra),
		response(
			success(),
			failure(fz::native_string error)
		)
	>;

	using generate_acme_certificate = command<struct generate_acme_certificate_tag
		(fz::acme::serve_challenges::how how_to_serve_challenges, std::string account_id, std::vector<std::string> hostnames, fz::tls_param key, fz::native_string key_password),
		response(
			success(fz::securable_socket::cert_info info, fz::securable_socket::cert_info::extra extra),
			failure(fz::native_string error)
		)
	>;

	using retrieve_update_raw_data = command<struct retrieve_update_raw_data_tag (std::string query_string), response_from_message (
		fz::rmp::make_message_t<fz::update::info::raw_data_retriever::result>
	)>;

	using solicit_update_info = message<struct solicit_update_info_tag()>;

	using update_info = fz::rmp::make_message_t <fz::update::checker::result>;

	using get_public_ip = command <struct get_public_ip_tag(fz::address_type at), response(
		success(std::string ip),
		failure(fz::native_string error)
	)>;

	// Temporarily modifies the ftp options and creates a temporary ftp user
	// The options are reverted to the previous ones, and the test environment is erased, in case the timeout expires.
	// If a set_ftp_options command is received before the expiration time, or if the settings are reloaded,
	// then the previous options are discarded and the the test environment is erased.
	using create_ftp_test_environment = command <struct create_ftp_test_environment_tag
		(fz::ftp::server::options ftp_options, fz::duration timeout),
		response(std::string username, std::string password)
	>;

	using destroy_ftp_test_environment = command <struct destroy_ftp_test_environment_tag(), response()>;

	// Feel free to add/remove/move around any of the following items, EXCEPT FOR admin_login and admin_login::response.
	// REMEMBER: increase the protocol_version if you modify the following list.
	// REMEMBER: admin_login is the "key" message, meaning it's the one that checks for the protocol version.
	struct any_message: fz::rmp::any_message<
		admin_login, admin_login::response,

		session::start,
		session::stop,
		session::user_name,
		session::entry_open,
		session::entry_close,
		session::entry_written,
		session::entry_read,
		session::protocol_info,
		session::solicit_info,

		log,

		server_status,
		listener_status,

		acknowledge_queue_full, acknowledge_queue_full::response,

		ban_ip,                ban_ip::response,
		set_server_status,     set_server_status::response,
		end_sessions,          end_sessions::response,
		get_groups_and_users,  get_groups_and_users::response,
		set_groups_and_users,  set_groups_and_users::response,
		get_ip_filters,        get_ip_filters::response,
		set_ip_filters,        set_ip_filters::response,
		set_ftp_options,       set_ftp_options::response,
		get_ftp_options,       get_ftp_options::response,
		set_protocols_options, set_protocols_options::response,
		get_protocols_options, get_protocols_options::response,
		set_admin_options,     set_admin_options::response,
		get_admin_options,     get_admin_options::response,
		set_logger_options,    set_logger_options::response,
		get_logger_options,    get_logger_options::response,
		set_acme_options,      set_acme_options::response,
		get_acme_options,      get_acme_options::response,
		set_pkcs11_options,    set_pkcs11_options::response,
		get_pkcs11_options,    get_pkcs11_options::response,
		set_updates_options,   set_updates_options::response,
		get_updates_options,   get_updates_options::response,
		set_webui_options,     set_webui_options::response,
		get_webui_options,     get_webui_options::response,
		destroy_webui_tokens,  destroy_webui_tokens::response,

		generate_selfsigned_certificate, generate_selfsigned_certificate::response,
		get_extra_certs_info,            get_extra_certs_info::response,
		get_deobfuscated_blob,           get_deobfuscated_blob::response,

		get_acme_terms_of_service, get_acme_terms_of_service::response,
		generate_acme_account,     generate_acme_account::response,
		restore_acme_account,      restore_acme_account::response,
		generate_acme_certificate, generate_acme_certificate::response,

		retrieve_update_raw_data, retrieve_update_raw_data::response,
		solicit_update_info,
		update_info,

		get_public_ip, get_public_ip::response,
		create_ftp_test_environment, create_ftp_test_environment::response,
		destroy_ftp_test_environment, destroy_ftp_test_environment::response
	>{};

	struct engine: fz::rmp::engine<any_message> {};

	static constexpr std::size_t buffer_size_before_login = 32*1024UL;
	static constexpr std::size_t buffer_size_after_login = 10*1024*1024UL;

}

#endif // ADMINISTRATION_HPP
