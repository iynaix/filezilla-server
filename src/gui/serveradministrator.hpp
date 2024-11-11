#ifndef WXFTPSERVERADMINISTRATOR_HPP
#define WXFTPSERVERADMINISTRATOR_HPP

#include <wx/panel.h>
#include <wx/timer.h>

#include "eventex.hpp"
#include "sessionlist.hpp"
#include "../server/administration.hpp"
#include "../filezilla/logger/splitter.hpp"
#include "../filezilla/update/raw_data_retriever/http.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

class ServerLogger;
class SettingsDialog;
class wxDialog;
class wxGUIEventLoop;

class ServerAdministrator: public wxPanel
{
public:
	struct server_info {
		std::string name = "localhost:14148";
		std::string host = "localhost";
		unsigned int port = 14148;
		std::string password{};
		std::string fingerprint{};

		#if defined ENABLE_ADMIN_DEBUG && ENABLE_ADMIN_DEBUG
			bool use_tls{};
		#endif

		template <typename Archive>
		void serialize(Archive &ar)
		{
			ar(
				FZ_NVP(name),
				FZ_NVP(host),
				FZ_NVP(port),
				FZ_NVP_O(password),
				FZ_NVP_O(fingerprint)

				#if defined ENABLE_ADMIN_DEBUG && ENABLE_ADMIN_DEBUG
					, FZ_NVP_O(use_tls)
				#endif
			);
		}
	};

	struct server_status {
		int num_of_active_sessions = -1; ///< If < 0, then we're not connected to the server.
	};

	ServerAdministrator(wxWindow *parent, fz::logger_interface &logger);
	~ServerAdministrator() override;

	void ConnectTo(const server_info &info, int reconnect_timeout_secs = 0 /* 0 == don't reconnect */);
	void Disconnect();

	bool IsConnected() const;

	void ConfigureServer();
	void ConfigureNetwork();
	void TestNetwork();
	void ExportConfig();
	void ImportConfig();

	#ifdef ENABLE_FZ_UPDATE_CHECKER
		bool CheckForUpdates();
	#endif

	const server_info &GetServerInfo() const;
	const server_status &GetServerStatus() const;

	struct Event: wxEventEx<Event> {
		inline const static Tag Connection;
		inline const static Tag Disconnection;
		inline const static Tag Reconnection;
		inline const static Tag ServerInfoUpdated;
		inline const static Tag ServerStatusChanged;

		int error{};
		bool server_initiated_disconnection{};

	private:
		friend Tag;
		using wxEventEx::wxEventEx;

		Event(Tag eventType, int err, bool server_initiated_disconnection = false);
		Event(Tag eventType);
	};

private:
	class Splitter;
	struct config_parts;

	fz::mutex mutex_;

	fz::logger::splitter logger_;

	mutable std::atomic<bool> is_being_deleted_{false};

	ServerLogger *server_logger_{};
	SessionList *session_list_{};
	SettingsDialog *settings_dialog_{};

	wxGUIEventLoop *rmp_loop_{};
	wxTimer *rmp_loop_timer_{};
	wxString rmp_loop_error_{};
	std::string rmp_loop_response_string_;
	fz::securable_socket::omni_cert_info *out_cert_info_{};
	fz::securable_socket::omni_cert_info::extra *out_cert_info_extra_{};
	std::pair<std::string, std::string> rmp_temp_user_and_pass_{};

	server_info server_info_{};
	server_status server_status_{};

	void maybe_send_server_status_changed(bool timer_stopped = false);
	fz::monotonic_clock last_server_status_changed_time_;
	wxTimer server_status_changed_timer_;
	bool server_status_timer_running_ = false;

	enum class state {
		disconnected,
		connecting,
		reconnecting,
		connected,
		disconnecting
	} state_ = state::disconnected;

	int reconnect_timeout_secs_{};
	wxTimer reconnect_timer_;
	bool certificate_not_trusted_{};

private:
	void do_connect();
	void do_disconnect(int err);
	void do_reconnect(int err);

private:
	void set_configure_opts();
	bool get_configure_opts();
	void resync_obfuscated_blobs_on_reconnect(Event &);

private:
	/******/

	void maybe_act_on_settings_received();
	fz::acme::extra_account_info acme_extra_account_info_{};
	fz::acme::extra_account_info working_acme_extra_account_info_{};
	void generate_acme_account();

	fz::expected<std::string, wxString> get_public_ip(fz::address_type at);
	fz::expected<std::pair<std::string, std::string>, wxString> create_ftp_test_environment(const fz::ftp::server::options &opts, fz::duration timeout);

	static wxSizer *create_session_info_sizer(wxWindow *p, const SessionList::Session::Info &i, const administration::session::any_protocol_info &a);

	class Dispatcher: public administration::engine::visitor<Dispatcher> {
	public:
		Dispatcher(ServerAdministrator &);

		//void operator()(administration::engine::session &session, administration::engine::any_message &&any) override;
		void connection(administration::engine::session *s, int err) override;
		void disconnection(administration::engine::session &s, int err) override;
		void certificate_verification(administration::engine::session &s, fz::tls_session_info &&i) override;

		friend class administration::engine::access;

		void operator()(administration::session::start && v);
		void operator()(administration::session::stop && v);
		void operator()(administration::session::user_name && v);
		void operator()(administration::session::entry_open && v);
		void operator()(administration::session::entry_close && v);
		void operator()(administration::session::entry_written && v);
		void operator()(administration::session::entry_read && v);
		void operator()(administration::session::protocol_info && v);
		void operator()(administration::listener_status && v);
		void operator()(administration::log &&v);

		void operator()(administration::end_sessions::response && v);
		void operator()(administration::get_groups_and_users::response && v);
		void operator()(administration::get_ip_filters::response && v);
		void operator()(administration::get_protocols_options::response && v);
		void operator()(administration::get_ftp_options::response && v);
		void operator()(administration::get_admin_options::response && v);
		void operator()(administration::get_logger_options::response && v);
		void operator()(administration::get_acme_options::response && v);
		void operator()(administration::get_pkcs11_options::response && v);
		void operator()(administration::admin_login::response && v, administration::engine::session &session);
		void operator()(administration::generate_selfsigned_certificate::response && v);
		void operator()(administration::generate_acme_certificate::response &&v);
		void operator()(administration::generate_acme_account::response &&v);
		void operator()(administration::get_acme_terms_of_service::response &&v, administration::engine::session &session);
		void operator()(administration::get_extra_certs_info::response &&v);
		void operator()(administration::get_deobfuscated_blob::response &&v);
		void operator()(administration::acknowledge_queue_full &&v, administration::engine::session &session);
		void operator()(administration::get_public_ip::response &&v);
		void operator()(administration::create_ftp_test_environment::response &&v);
		void operator()(administration::get_webui_options::response && v);
		void operator()(administration::destroy_webui_tokens::response && v);

		#if !defined(WITHOUT_FZ_UPDATE_CHECKER)
			void operator()(administration::get_updates_options::response &&v);
			void operator()(administration::update_info &&v);
			void operator()(administration::retrieve_update_raw_data &&v);
		#endif

		void operator()(const administration::exception::generic &);

	private:
		template <typename F>
		void invoke_on_admin(F && f) {
			if (disabled_receiving_count_++ == 0)
				server_admin_.client_.enable_receiving(false);

			server_admin_.CallAfter([f = std::forward<F>(f), this]() mutable {
				f();

				if (--disabled_receiving_count_ == 0)
					server_admin_.client_.enable_receiving(true);
			});
		}

		bool was_expecting_response();

		ServerAdministrator &server_admin_;
		std::atomic<std::size_t> disabled_receiving_count_{};
		std::unique_ptr<fz::async_handler> async_handler_;
		fz::update::raw_data_retriever::http http_update_retriever_;
	};

	fz::event_loop loop_;
	fz::thread_pool pool_;
	fz::tls_system_trust_store trust_store_;
	Dispatcher dispatcher_;
	administration::engine::client client_;

	bool waiting_for_updates_info_{};
	std::size_t responses_to_wait_for_{};
	fz::authentication::file_based_authenticator::users users_;
	fz::authentication::file_based_authenticator::groups groups_;
	fz::util::fs::path_format server_path_format_;
	bool server_can_impersonate_{};
	fz::native_string server_username_;
	std::optional<std::vector<fz::network_interface>> network_interfaces_;
	bool any_is_equivalent_{};
	fz::build_info::version_info server_version_{};
	std::string server_host_{};
	std::vector<uint8_t> server_instance_id_;

	fz::update::info server_update_info_{};
	fz::datetime server_update_last_check_{};
	fz::datetime server_update_next_check_{};

	fz::tcp::binary_address_list disallowed_ips_;
	fz::tcp::binary_address_list allowed_ips_;
	server_settings::protocols_options protocols_options_;
	fz::ftp::server::options ftp_options_;
	fz::webui::server::options webui_options_;
	fz::logger::file::options logger_options_;
	server_settings::admin_options admin_options_;
	server_settings::acme_options acme_options_;
	server_settings::pkcs11_options pkcs11_options_;
#ifdef ENABLE_FZ_UPDATE_CHECKER
	fz::update::checker::options updates_options_;
#endif
	fz::securable_socket::cert_info::extra ftp_tls_extra_info_{};
	fz::securable_socket::cert_info::extra webui_tls_extra_info_{};
	fz::securable_socket::cert_info::extra admin_tls_extra_info_{};

	struct public_ip_cache {
		std::string ip;

		void reset(fz::monotonic_clock at = {});
		bool expired();

	private:
		fz::monotonic_clock time;
	};

	public_ip_cache public_ipvx_cache_;
	public_ip_cache public_ipv4_cache_;
	public_ip_cache public_ipv6_cache_;

	std::function<void()> on_settings_received_func_;
};

FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_acme_options::response);
FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_acme_certificate::response);
FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_acme_account::response);
FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_acme_terms_of_service::response);

FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::generate_selfsigned_certificate::response);
FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_extra_certs_info::response);

#ifdef ENABLE_FZ_UPDATE_CHECKER
	FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_updates_options::response);
	FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::update_info);
	FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::retrieve_update_raw_data);
#endif

FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_groups_and_users::response);
FZ_RMP_INSTANTIATE_EXTERNALLY_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::create_ftp_test_environment::response);

#endif // WXFTPSERVERADMINISTRATOR_HPP
