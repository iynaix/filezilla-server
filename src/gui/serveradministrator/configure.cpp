#include <libfilezilla/glue/wx.hpp>

#include <wx/splitter.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/sizer.h>
#include <wx/log.h>
#include <wx/statline.h>
#include <wx/app.h>
#include <wx/hyperlink.h>
#include <wx/statbmp.h>
#include <wx/choicebk.h>
#include <wx/filepicker.h>
#include <wx/evtloop.h>
#include <wx/checkbox.h>

#include "../serveradministrator.hpp"
#include "../settings.hpp"
#include "../certinfoeditor.hpp"
#include "../settingsdialog.hpp"
#include "../networkconfigwizard.hpp"
#include "../textvalidatorex.hpp"

#include "../locale.hpp"
#include "../glue.hpp"
#include "../helpers.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

void ServerAdministrator::set_configure_opts()
{
	if (!settings_dialog_)
		return;

	settings_dialog_->SetServerInstanceId(server_instance_id_);
	settings_dialog_->SetHostaddressAnyIsEquivalent(any_is_equivalent_);

	settings_dialog_->SetGroupsAndUsers(groups_, users_, server_can_impersonate_, server_username_);
	settings_dialog_->SetFilters(disallowed_ips_, allowed_ips_);
	settings_dialog_->SetProtocolsOptions(protocols_options_);
	settings_dialog_->SetAdminOptions(admin_options_, admin_tls_extra_info_);
	settings_dialog_->SetFtpOptions(ftp_options_, ftp_tls_extra_info_);
	settings_dialog_->SetLoggingOptions(logger_options_);
	settings_dialog_->SetAcmeOptions(acme_options_, acme_extra_account_info_);
	settings_dialog_->SetPkcs11Options(pkcs11_options_);

#ifdef ENABLE_FZ_WEBUI
	settings_dialog_->SetWebUIOptions(webui_options_, webui_tls_extra_info_);
#endif

#ifdef ENABLE_FZ_UPDATE_CHECKER
	settings_dialog_->SetUpdatesOptions(updates_options_);
	settings_dialog_->SetUpdateInfo(server_update_info_, server_update_last_check_, server_update_next_check_);
#endif
}

bool ServerAdministrator::get_configure_opts()
{
	if (!settings_dialog_)
		return false;

	std::tie(groups_, users_) = settings_dialog_->GetGroupsAndUsers();
	std::tie(disallowed_ips_, allowed_ips_) = settings_dialog_->GetFilters();
	protocols_options_ = settings_dialog_->GetProtocolsOptions();

	std::tie(ftp_options_, ftp_tls_extra_info_) = settings_dialog_->GetFtpOptions();
	std::tie(admin_options_, admin_tls_extra_info_) = settings_dialog_->GetAdminOptions();

	logger_options_ = settings_dialog_->GetLoggingOptions();
	acme_options_ = settings_dialog_->GetAcmeOptions();
	pkcs11_options_ = settings_dialog_->GetPkcs11Options();

#ifdef ENABLE_FZ_WEBUI
	std::tie(webui_options_, webui_tls_extra_info_) = settings_dialog_->GetWebUIOptions();
#endif

#ifdef ENABLE_FZ_UPDATE_CHECKER
	updates_options_ = settings_dialog_->GetUpdatesOptions();
#endif

	return true;
}

void ServerAdministrator::resync_obfuscated_blobs_on_reconnect(Event &ev)
{
	ev.Skip();

	if (GetServerStatus().num_of_active_sessions < 0) {
		return;
	}

	wxCHECK_RET(IsConnected(), wxT("The Admin Interface is not connected, but it should."));
	wxCHECK_RET(settings_dialog_ != nullptr, wxT("The Settings dialog should be open, but it's not."));
	wxCHECK_RET(responses_to_wait_for_ == 0, wxT("The Admin Interface is waiting for responses from the server already, but it shouldn't [1]."));
	wxCHECK_RET(!on_settings_received_func_, wxT("The Admin Interface is waiting for responses from the server already, but it shouldn't [2]."));

	if (server_instance_id_ == settings_dialog_->GetServerInstanceId()) {
		logger_.log_raw(fz::logmsg::debug_info, L"The server reconnected, but instance ID has not changed. No need to perform any resync.");
		return;
	}

	bool ftp_cert_currently_obfuscated = settings_dialog_->IsFtpCertCurrentlyObfuscatedAndNotModified();
	bool admin_cert_currently_obfuscated = settings_dialog_->IsAdminCertCurrentlyObfuscatedAndNotModified();

	#ifdef ENABLE_FZ_WEBUI
		bool webui_cert_currently_obfuscated = settings_dialog_->IsWebUICertCurrentlyObfuscatedAndNotModified();
	#else
		bool webui_cert_currently_obfuscated = false;
	#endif

	if (ftp_cert_currently_obfuscated || webui_cert_currently_obfuscated || admin_cert_currently_obfuscated) {
		on_settings_received_func_ = [this, ftp_cert_currently_obfuscated, webui_cert_currently_obfuscated, admin_cert_currently_obfuscated] {
			auto resync = [this](auto && func, auto && server_cert_info, auto && what) {
				if ((settings_dialog_->*func)(server_cert_info)) {
					logger_.log_u(fz::logmsg::debug_info, L"Obfuscated %s certificate key resynced with the server.", what);
				}
				else {
					logger_.log_u(fz::logmsg::debug_warning, L"Obfuscated %s certificate key could not be resynced with the server, which makes it unusable.", what);
				}
			};

			if (ftp_cert_currently_obfuscated) {
				resync(&SettingsDialog::SetFtpObfuscatedCert, ftp_options_.sessions().tls.cert, "FTP");
			}

			if (webui_cert_currently_obfuscated) {
				#ifdef ENABLE_FZ_WEBUI
					resync(&SettingsDialog::SetWebUIObfuscatedCert, webui_options_.tls.cert, "WebUI");
				#endif
			}

			if (admin_cert_currently_obfuscated) {
				resync(&SettingsDialog::SetAdminObfuscatedCert, admin_options_.tls.cert, "Administration");
			}

			on_settings_received_func_ = nullptr;
			settings_dialog_->Enable();
		};

		settings_dialog_->Disable();

		if (ftp_cert_currently_obfuscated) {
			responses_to_wait_for_ += 1;
			logger_.log_raw(fz::logmsg::debug_info, L"Resyncing obfuscated FTP certificate key with the server...");

			client_.send<administration::get_ftp_options>();
		}

		if (webui_cert_currently_obfuscated) {
			responses_to_wait_for_ += 1;
			logger_.log_raw(fz::logmsg::debug_info, L"Resyncing obfuscated WebUI certificate key with the server...");

			client_.send<administration::get_webui_options>();
		}

		if (admin_cert_currently_obfuscated) {
			responses_to_wait_for_ += 1;
			logger_.log_raw(fz::logmsg::debug_info, L"Resyncing obfuscated Admin certificate key with the server...");

			client_.send<administration::get_admin_options>();
		}
	}
}

void ServerAdministrator::ConfigureServer()
{
	if (!IsConnected())
		return;

	if (responses_to_wait_for_ > 0) {
		logger_.log_u(fz::logmsg::debug_warning, L"Still retrieving server's configuration");
		return;
	}

	on_settings_received_func_ = [this] {
		logger_.log_u(fz::logmsg::status, _S("Server's configuration retrieved."));

		wxPushDialog<SettingsDialog>(this, fz::to_wxString(server_info_.name), server_path_format_) | [this](SettingsDialog &d) {
			on_settings_received_func_ = nullptr;

			settings_dialog_ = &d;

			set_configure_opts();

			Bind(Event::ServerStatusChanged, &ServerAdministrator::resync_obfuscated_blobs_on_reconnect, this);

			settings_dialog_->SetApplyFunction([this] {
				if (!client_.is_connected()) {
					wxMsg::Error(_S("Got disconnected from the server, cannot apply the changes.\n\nPlease, try again later."));
					return false;
				}

				if (!get_configure_opts()) {
					return false;
				}

				// Update the stored server's admin protocol TLS cert fingerprint.
				if (auto &&f = admin_options_.tls.cert.fingerprint(admin_tls_extra_info_); !f.empty() && server_info_.fingerprint != f) {
					server_info_.fingerprint = std::move(f);
					Event::ServerInfoUpdated.Process(this, this);
				}

				client_.send<administration::set_groups_and_users>(groups_, users_, true);
				client_.send<administration::set_ip_filters>(disallowed_ips_, allowed_ips_);
				client_.send<administration::set_protocols_options>(protocols_options_);
				client_.send<administration::set_admin_options>(admin_options_);
				client_.send<administration::set_ftp_options>(ftp_options_);
				client_.send<administration::set_logger_options>(logger_options_);
				client_.send<administration::set_acme_options>(acme_options_);
				client_.send<administration::set_pkcs11_options>(pkcs11_options_);

#ifdef ENABLE_FZ_WEBUI
				client_.send<administration::set_webui_options>(webui_options_);
#endif

#ifdef ENABLE_FZ_UPDATE_CHECKER
				client_.send<administration::set_updates_options>(updates_options_);
#endif
				return true;
			});

			settings_dialog_->SetGenerateSelfsignedCertificateFunction([this](const std::string &dn, const std::vector<std::string> &hostnames, fz::tls_param key, fz::native_string password,
				fz::securable_socket::omni_cert_info &out_info, fz::securable_socket::omni_cert_info::extra &out_extra
			) -> wxString {
				wxCHECK_MSG(rmp_loop_ == nullptr, wxT("Internal error: rmp_loop_ is not NULL"), wxT("Internal error: rmp_loop_ is not NULL"));

				if (!client_.is_connected()) {
					return _S("Got disconnected from the server, try again later.");
				}

				wxGUIEventLoop loop;
				wxTimer timer;
				wxWindowDisabler disabler;

				timer.Bind(wxEVT_TIMER, [this, &loop](wxTimerEvent &) {
					rmp_loop_error_ = _S("Timed out waiting for response from server.");
					loop.Exit();
				});

				rmp_loop_ = &loop;
				rmp_loop_timer_ = &timer;
				out_cert_info_ = &out_info;
				out_cert_info_extra_ = &out_extra;

				logger_.log_u(fz::logmsg::status, L"%s", _S("Generating self-signed certificate..."));
				client_.send<administration::generate_selfsigned_certificate>(dn, hostnames, std::move(key), std::move(password));

				timer.Start(20000, true);

				loop.Run();

				rmp_loop_  = nullptr;
				rmp_loop_timer_ = nullptr;
				out_cert_info_ = nullptr;
				out_cert_info_extra_ = nullptr;

				return rmp_loop_error_;

			});

			settings_dialog_->SetGenerateAcmeCertificateFunction([this](const std::vector<std::string> &hostnames, const fz::tls_param &key, const fz::native_string &key_password,
				fz::securable_socket::omni_cert_info &out_info, fz::securable_socket::omni_cert_info::extra &out_extra
			) -> wxString {
				wxCHECK_MSG(rmp_loop_ == nullptr, wxT("Internal error: rmp_loop_ is not NULL"), wxT("Internal error: rmp_loop_ is not NULL"));

				if (!client_.is_connected()) {
					return _S("Got disconnected from the server, try again later.");
				}

				wxGUIEventLoop loop;
				wxTimer timer;
				wxWindowDisabler disabler;

				timer.Bind(wxEVT_TIMER, [this, &loop](wxTimerEvent &) {
					rmp_loop_error_ = _S("Timed out waiting for response from server.");
					loop.Exit();
				});


				rmp_loop_ = &loop;
				rmp_loop_timer_ = &timer;
				out_cert_info_ = &out_info;
				out_cert_info_extra_ = &out_extra;

				auto opts = settings_dialog_->GetAcmeOptions();
				logger_.log_u(fz::logmsg::status, L"%s", _S("Generating ACME certificate..."));
				client_.send<administration::generate_acme_certificate>(opts.how_to_serve_challenges, opts.account_id, hostnames, key, key_password);

				timer.Start(20000, true);

				loop.Run();

				rmp_loop_  = nullptr;
				rmp_loop_timer_ = nullptr;
				out_cert_info_ = nullptr;
				out_cert_info_extra_ = nullptr;

				return rmp_loop_error_;
			});

			settings_dialog_->SetTestCertificateFunction([this](const fz::securable_socket::cert_info &info,
				fz::securable_socket::omni_cert_info::extra &out_extra
			) -> wxString {
				wxCHECK_MSG(rmp_loop_ == nullptr, wxT("Internal error: rmp_loop_ is not NULL"), wxT("Internal error: rmp_loop_ is not NULL"));

				if (!client_.is_connected()) {
					return _S("Got disconnected from the server, cannot apply the changes.\n\nPlease, try again later.");
				}

				wxGUIEventLoop loop;
				wxTimer timer;
				wxWindowDisabler disabler;

				timer.Bind(wxEVT_TIMER, [this, &loop](wxTimerEvent &) {
					rmp_loop_error_ = _S("Timed out waiting for response from server.");
					loop.Exit();
				});


				rmp_loop_ = &loop;
				rmp_loop_timer_ = &timer;
				out_cert_info_extra_ = &out_extra;

				logger_.log_u(fz::logmsg::status, L"%s", _S("Getting certificate info..."));
				client_.send<administration::get_extra_certs_info>(info);
				timer.Start(20000, true);

				loop.Run();

				rmp_loop_  = nullptr;
				rmp_loop_timer_ = nullptr;
				out_cert_info_extra_ = nullptr;

				if (rmp_loop_error_ == wxT("Could not deobfuscate the private key.")) {
					//rmp_loop_error_ = _S("The server has been restarted")
				}
				return rmp_loop_error_;
			});

			settings_dialog_->SetRetrieveDeobfuscatedBlobFunction([this](std::string_view obfuscated) -> fz::expected<std::string, wxString> {
				wxCHECK_MSG(rmp_loop_ == nullptr, fz::unexpected(wxT("Internal error: rmp_loop_ is not NULL")), wxT("Internal error: rmp_loop_ is not NULL"));

				if (!client_.is_connected()) {
					return fz::unexpected(_S("Got disconnected from the server, try again later."));
				}

				wxGUIEventLoop loop;
				wxTimer timer;
				wxWindowDisabler disabler;

				timer.Bind(wxEVT_TIMER, [&loop, this](wxTimerEvent &) {
					rmp_loop_error_ = _S("Timed out waiting for response from server.");
					loop.Exit();
				});

				rmp_loop_ = &loop;
				rmp_loop_timer_ = &timer;

				rmp_loop_error_.clear();

				client_.send<administration::get_deobfuscated_blob>(std::string(obfuscated));
				timer.Start(20000, true);

				loop.Run();

				rmp_loop_  = nullptr;
				rmp_loop_timer_ = nullptr;

				if (!rmp_loop_error_.empty() || rmp_loop_response_string_.empty()) {
					return fz::unexpected(std::move(rmp_loop_error_));
				}

				return rmp_loop_response_string_;
			});

#ifdef ENABLE_FZ_WEBUI
			settings_dialog_->SetDestroyWebUITokensFunction([this]() -> wxString {
				wxCHECK_MSG(rmp_loop_ == nullptr, wxT("Internal error: rmp_loop_ is not NULL"), wxT("Internal error: rmp_loop_ is not NULL"));

				if (!client_.is_connected()) {
					return _S("Got disconnected from the server, try again later.");
				}

				wxGUIEventLoop loop;
				wxTimer timer;
				wxWindowDisabler disabler;

				timer.Bind(wxEVT_TIMER, [&loop, this](wxTimerEvent &) {
					rmp_loop_error_ = _S("Timed out waiting for response from server.");
					loop.Exit();
				});

				rmp_loop_ = &loop;
				rmp_loop_timer_ = &timer;

				rmp_loop_error_.clear();

				client_.send<administration::destroy_webui_tokens>();
				timer.Start(20000, true);

				loop.Run();

				rmp_loop_  = nullptr;
				rmp_loop_timer_ = nullptr;

				if (!rmp_loop_error_.empty()) {
					return std::move(rmp_loop_error_);
				}

				return wxEmptyString;
			});
#endif
			settings_dialog_->SetGetPublicIpFunc([this](fz::address_type at){return get_public_ip(at);});

			settings_dialog_->SetGenerateAcmeAccountFunction([this] { generate_acme_account(); });

#ifdef ENABLE_FZ_UPDATE_CHECKER
			settings_dialog_->SetUpdateCheckFunc([this] {
				return CheckForUpdates();
			});
#endif
			settings_dialog_->ShowModal();
			settings_dialog_ = nullptr;

			Unbind(Event::ServerStatusChanged, &ServerAdministrator::resync_obfuscated_blobs_on_reconnect, this);
		};
	};

	responses_to_wait_for_ = 8;

	logger_.log_u(fz::logmsg::status, _S("Retrieving configuration from the server..."));

	client_.send<administration::get_groups_and_users>();
	client_.send<administration::get_ip_filters>();
	client_.send<administration::get_protocols_options>();
	client_.send<administration::get_admin_options>();
	client_.send<administration::get_ftp_options>();
	client_.send<administration::get_logger_options>();
	client_.send<administration::get_acme_options>();
	client_.send<administration::get_pkcs11_options>();

#ifdef ENABLE_FZ_WEBUI
	responses_to_wait_for_ += 1;
	client_.send<administration::get_webui_options>();
#endif

#ifdef ENABLE_FZ_UPDATE_CHECKER
	responses_to_wait_for_ += 1;
	client_.send<administration::get_updates_options>();
#endif
}

void ServerAdministrator::ConfigureNetwork()
{
	if (!IsConnected())
		return;

	if (responses_to_wait_for_ > 0) {
		logger_.log_u(fz::logmsg::debug_warning, L"Still retrieving server's configuration");
		return;
	}

	on_settings_received_func_ = [this] {
		wxPushDialog<NetworkConfigWizard>(this, std::ref(pool_), std::ref(loop_), &trust_store_, std::ref(logger_), _F("Network configuration wizard for server %s", fz::to_wxString(server_info_.name))) | [this](NetworkConfigWizard &wiz) {
			on_settings_received_func_ = nullptr;

			wiz.SetFtpOptions(ftp_options_);
			wiz.SetGetPublicIpFunc([this](fz::address_type at){return get_public_ip(at);});
			wiz.SetCreateFtpTestEnvironmentFunc([this](const fz::ftp::server::options &opts){return create_ftp_test_environment(opts, fz::duration::from_minutes(5));});

			if (wiz.Run()) {
				if (!client_.is_connected()) {
					wxMsg::Error(_S("Got disconnected from the server, cannot apply the changes.\n\nPlease, try again later."));
					return;
				}

				client_.send<administration::set_ftp_options>(wiz.GetFtpOptions());
			}
			else {
				client_.send<administration::destroy_ftp_test_environment>();
			}
		};
	};

	responses_to_wait_for_ = 1;
	client_.send<administration::get_ftp_options>();
}

void ServerAdministrator::TestNetwork()
{
	if (!IsConnected())
		return;

	if (responses_to_wait_for_ > 0) {
		logger_.log_u(fz::logmsg::debug_warning, L"Still retrieving server's configuration");
		return;
	}

	on_settings_received_func_ = [this] {
		wxPushDialog(this, nullID, _F("FTP configuration tester for server %s", fz::to_wxString(server_info_.name)), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER) | [this](auto *p) {
			on_settings_received_func_ = nullptr;

			FtpTester *tester;

			wxVBox(p) = {
				wxSizerFlags(1) >>= tester = new FtpTester(p, pool_, loop_, &trust_store_, logger_),

				new wxStaticLine(p),
				p->CreateButtonSizer(wxCLOSE)
			};

			tester->SetFtpOptions(&ftp_options_);
			tester->SetGetPublicIpFunc([this](fz::address_type at){return get_public_ip(at);});
			tester->SetCreateFtpTestEnvironmentFunc([this](const fz::ftp::server::options &opts){return create_ftp_test_environment(opts, fz::duration::from_minutes(5));});

			p->ShowModal();
			client_.send<administration::destroy_ftp_test_environment>();
		};
	};

	responses_to_wait_for_ = 1;
	client_.send<administration::get_ftp_options>();
}
