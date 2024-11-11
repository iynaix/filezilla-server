#ifndef WXFZFTPSETTINGSDIALOG_HPP
#define WXFZFTPSETTINGSDIALOG_HPP

#include <wx/propdlg.h>
#include <wx/checkbox.h>

#include "groupslist.hpp"
#include "certinfoeditor.hpp"

#include "dialogex.hpp"

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "../server/server_settings.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

class AuthenticationEditor;
class PasswordEditor;
class FtpEditor;
class IntegralEditor;
class AddressInfoListEditor;
class FiltersEditor;
class GroupsEditor;
class UsersEditor;
class wxChoice;

class SettingsDialog: public wxDialogEx<wxPropertySheetDialog>
{
public:
	using GetPublicIpFunc = std::function<fz::expected<std::string, wxString>(fz::address_type at)>;

	SettingsDialog();

	bool Create(wxWindow *parent,
				const wxString &server_name,
				fz::util::fs::path_format server_path_format,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER,
				const wxString& name = wxS("SettingsDialog"));

	void SetServerInstanceId(const std::vector<uint8_t> id);
	const std::vector<uint8_t> &GetServerInstanceId() const;

	void SetHostaddressAnyIsEquivalent(bool any_is_equivalent);

	void SetGroupsAndUsers(const fz::authentication::file_based_authenticator::groups &groups, const fz::authentication::file_based_authenticator::users &users, bool can_impersonate, const fz::native_string server_username);
	std::pair<const fz::authentication::file_based_authenticator::groups &, const fz::authentication::file_based_authenticator::users &> GetGroupsAndUsers() const;

	void SetProtocolsOptions(const server_settings::protocols_options &protocols_options);
	const server_settings::protocols_options &GetProtocolsOptions() const;

	void SetFtpOptions(const fz::ftp::server::options &ftp_options, const fz::securable_socket::cert_info::extra &tls_extra_info);
	std::pair<const fz::ftp::server::options &, const fz::securable_socket::cert_info::extra &> GetFtpOptions() const;
	bool IsFtpCertCurrentlyObfuscatedAndNotModified();
	bool SetFtpObfuscatedCert(const fz::securable_socket::cert_info &cert_info);

	void SetAdminOptions(const server_settings::admin_options &admin_options, const fz::securable_socket::cert_info::extra &tls_extra_info);
	std::pair<const server_settings::admin_options &, const fz::securable_socket::cert_info::extra &> GetAdminOptions() const;
	bool IsAdminCertCurrentlyObfuscatedAndNotModified();
	bool SetAdminObfuscatedCert(const fz::securable_socket::cert_info &cert_info);


#ifdef ENABLE_FZ_WEBUI
	void SetWebUIOptions(const fz::webui::server::options &webui_options, const fz::securable_socket::cert_info::extra &tls_extra_info);
	std::pair<const fz::webui::server::options &, const fz::securable_socket::cert_info::extra &> GetWebUIOptions() const;
	bool IsWebUICertCurrentlyObfuscatedAndNotModified();
	bool SetWebUIObfuscatedCert(const fz::securable_socket::cert_info &cert_info);
	void SetDestroyWebUITokensFunction(std::function<wxString()> func);
#endif

	void SetFilters(const fz::tcp::binary_address_list &disallowed_ips, const fz::tcp::binary_address_list &allowed_ips);
	std::pair<const fz::tcp::binary_address_list &, const fz::tcp::binary_address_list &> GetFilters() const;

	void SetLoggingOptions(const fz::logger::file::options &opts);
	const fz::logger::file::options &GetLoggingOptions() const;

	void SetAcmeAccountId(const std::string &id, const fz::acme::extra_account_info &extra);
	void SetAcmeOptions(const server_settings::acme_options &acme_options, const fz::acme::extra_account_info &extra);
	const server_settings::acme_options &GetAcmeOptions() const;

	void SetPkcs11Options(const server_settings::pkcs11_options &pkcs11_options);
	const server_settings::pkcs11_options &GetPkcs11Options() const;

#ifdef ENABLE_FZ_UPDATE_CHECKER
	void SetUpdatesOptions(const fz::update::checker::options &opts);
	const fz::update::checker::options &GetUpdatesOptions() const;
	void SetUpdateInfo(const fz::update::info &info, fz::datetime last_check, fz::datetime next_check);
	using UpdateCheckFunc = std::function<bool()>;
	void SetUpdateCheckFunc(UpdateCheckFunc func);
#endif

	void SetApplyFunction(std::function<bool ()> func);
	void SetGenerateSelfsignedCertificateFunction(CertInfoEditor::GenerateSelfsignedFunc func);
	void SetGenerateAcmeCertificateFunction(CertInfoEditor::GenerateAcmeFunc func);
	void SetTestCertificateFunction(CertInfoEditor::TestCertificateFunc func);
	void SetRetrieveDeobfuscatedBlobFunction(CertInfoEditor::RetrieveDeobfuscatedBlobFunc func);

	using GenerateAcmeAccountFunc = std::function<void()>;
	void SetGenerateAcmeAccountFunction(GenerateAcmeAccountFunc func);

	void SetGetPublicIpFunc(GetPublicIpFunc func);

private:
	bool TransferDataFromWindow() override;
	wxBookCtrlBase* CreateBookCtrl() override;
	void AddBookCtrl(wxSizer *sizer) override;

	void SetLogTypes(fz::logmsg::type types);
	fz::logmsg::type GetLogTypes();

	void OnGroupsListChanging(GroupsList::Event &ev);

	void check_expired_certs();

	std::string server_name_;
	fz::util::fs::path_format server_path_format_{};
	std::vector<uint8_t> server_instance_id_{};
	bool any_is_equivalent_{};

	fz::authentication::file_based_authenticator::groups groups_;
	fz::authentication::file_based_authenticator::users users_;
	bool can_impersonate_{};
	fz::native_string server_username_{};

	fz::tcp::binary_address_list disallowed_ips_;
	fz::tcp::binary_address_list allowed_ips_;

	fz::ftp::server::options ftp_options_;
	server_settings::admin_options admin_options_;
	fz::logger::file::options logger_opts_;
	server_settings::acme_options acme_opts_;
	fz::acme::extra_account_info acme_extra_account_info_;
	server_settings::pkcs11_options pkcs11_options_;

#ifdef ENABLE_FZ_WEBUI
	fz::webui::server::options webui_options_;
	std::function<wxString()> webui_destroy_tokens_func_;
	fz::securable_socket::cert_info::extra webui_tls_extra_info_;
	wxChoice *webui_tls_min_ver_ctrl_{};
	CertInfoEditor *webui_cert_info_ctrl_{};
	IntegralEditor *webui_access_token_timeout_ctrl_{};
	IntegralEditor *webui_refresh_token_timeout_ctrl_{};
#endif

#ifdef ENABLE_FZ_UPDATE_CHECKER
	fz::update::checker::options updates_options_;
	fz::update::info update_info_;
	fz::datetime update_last_check_;
	fz::datetime update_next_check_;
	UpdateCheckFunc update_check_func_;
	wxButton *update_check_button_{};
#endif

	fz::securable_socket::cert_info::extra ftp_tls_extra_info_;
	fz::securable_socket::cert_info::extra admin_tls_extra_info_;

	server_settings::protocols_options protocols_options_;

	wxTextCtrl *welcome_message_ctrl_{};

	AddressInfoListEditor *server_listeners_editor_{};
	IntegralEditor *autoban_max_login_failures_ctrl_{};
	IntegralEditor *autoban_login_failures_time_window_ctrl_{};
	IntegralEditor *autoban_ban_duration_ctrl_{};
	IntegralEditor *performance_number_of_session_threads_ctrl_{};
	IntegralEditor *performance_receiving_buffer_size_ctrl_{};
	IntegralEditor *performance_sending_buffer_size_ctrl_{};
	wxCheckBox *server_receiving_buffer_size_check_{};
	wxCheckBox *server_sending_buffer_size_check_{};
	IntegralEditor *login_timeout_ctrl_{};
	IntegralEditor *activity_timeout_ctrl_{};

	wxWindow *pasv_page_{};
	wxCheckBox *use_custom_port_range_ctrl_{};
	IntegralEditor *min_port_range_ctrl_{};
	IntegralEditor *max_port_range_ctrl_{};
	wxTextCtrl *host_override_ctrl_{};
	wxCheckBox *disallow_host_override_for_local_peers_ctrl_{};
	GetPublicIpFunc get_public_ip_func_{};

	wxChoice *ftp_tls_min_ver_ctrl_{};
	CertInfoEditor *ftp_cert_info_ctrl_{};

	FiltersEditor *filters_ctrl_{};

	CertInfoEditor *admin_cert_info_ctrl_{};

	AddressInfoListEditor *admin_listeners_editor_{};
	IntegralEditor *admin_port_ctrl_{};
	PasswordEditor *admin_password_ctrl_{};

	wxWindow *authentication_page_{};
	wxSimplebook *default_impersonator_book_{};
	wxTextCtrl *default_impersonator_name_msw_{};
	wxTextCtrl *default_impersonator_password_msw_{};
	wxTextCtrl *default_impersonator_name_nix_{};
	wxTextCtrl *default_impersonator_group_nix_{};

	GroupsEditor *groups_editor_{};
	UsersEditor *users_editor_{};

	wxChoicebook *logging_choice_ctrl_{};
	wxTextCtrl *log_path_ctrl_{};
	IntegralEditor *log_rotations_amount_ctrl_{};
	wxCheckBox *log_date_in_name_{};
	IntegralEditor *log_file_size_ctrl_{};
	wxCheckBox *log_include_headers_ctrl_{};
	wxChoice *log_level_ctrl_{};
	wxChoicebook *log_rotation_type_ctrl_{};
	wxChoicebook *log_rotation_choice_ctrl_{};
	int log_level_old_selection_{};

	wxCheckBox *acme_enabled_{};
	wxChoicebook *acme_how_ctrl_{};
	wxTextCtrl  *acme_well_knon_path_ctrl_{};
	wxCheckBox *acme_create_path_if_not_exist_ctrl_{};
	AddressInfoListEditor *acme_listeners_ctrl_{};
	wxTextCtrl *acme_account_id_ctrl_{};
	wxTextCtrl *acme_account_directory_ctrl_{};
	wxTextCtrl *acme_account_contacts_ctrl_{};
	wxButton *acme_account_id_button_{};
	std::vector<fz::tcp::address_info> acme_listeners_;
	fz::native_string acme_well_known_path_;
	GenerateAcmeAccountFunc acme_generate_account_func_;

	std::function<bool()> apply_function_{};
};


#endif // WXFZFTPSETTINGSDIALOG_HPP
