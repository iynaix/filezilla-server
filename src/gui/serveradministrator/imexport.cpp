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

#include "../locale.hpp"
#include "../glue.hpp"
#include "../helpers.hpp"

#include "../tools/configconverter/server_config.hpp"
#include "../tools/configconverter/converter.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

namespace {

struct Checks: public wxCheckBoxGroup
{
	using wxCheckBoxGroup::wxCheckBoxGroup;

	CB listeners_and_protocols = c(_S("Server listeners and protocols"));
	CB rights_management       = c(_S("Rights management"));
	CB administration          = c(_S("Administration"));
	CB logging                 = c(_S("Logging"));
	CB acme                    = c(_S("Let's Encrypt®"));
	CB pkcs11                  = c(_S("PKCS#11"));

	#ifdef ENABLE_FZ_UPDATE_CHECKER
		CB updates = c(_S("Updates"));
	#endif
};

}

struct ServerAdministrator::config_parts
{
	std::optional<decltype(groups_)> groups;
	std::optional<decltype(users_)> users;
	std::optional<decltype(disallowed_ips_)> disallowed_ips;
	std::optional<decltype(allowed_ips_)> allowed_ips;
	std::optional<decltype(protocols_options_)> protocols_options;
	std::optional<decltype(ftp_options_)> ftp_options;
	std::optional<decltype(webui_options_)> webui_options;
	std::optional<decltype(admin_options_)> admin_options;
	std::optional<decltype(logger_options_)> logger_options;
	std::optional<decltype(acme_options_)> acme_options;
	std::optional<decltype(acme_extra_account_info_)> acme_extra_account_info;
	std::optional<decltype(pkcs11_options_)> pkcs11_options;

#ifdef ENABLE_FZ_UPDATE_CHECKER
	std::optional<decltype(updates_options_)> updates_options;
#endif

	std::optional<wxString> load(fz::native_string_view src);

private:
	struct no_error { std::string messages; };
	struct root_node_missing {};
	struct flavour_or_version_mismatch { std::string description; };
	struct other_error { std::string description; };

	struct loading_error: std::variant<
		no_error,
		root_node_missing,
		flavour_or_version_mismatch,
		other_error
	> {
		using variant::variant;

		const struct root_node_missing *root_node_missing() const { return std::get_if<struct root_node_missing>(this); }
		const struct flavour_or_version_mismatch *flavour_or_version_mismatch() const { return std::get_if<struct flavour_or_version_mismatch>(this); }
		const struct other_error *other_error() const { return std::get_if<struct other_error>(this); }
		const struct no_error *no_error() const { return std::get_if<struct no_error>(this); }

		explicit operator bool() const { return !std::get_if<struct no_error>(this); }
	};


	loading_error load_current(fz::serialization::xml_input_archive::file_loader &loader,  fz::serialization::xml_input_archive::options::verify_mode verify_mode);
	loading_error load_ancient(fz::serialization::xml_input_archive::file_loader &loader);
};

void ServerAdministrator::ExportConfig()
{
	if (!IsConnected())
		return;

	if (responses_to_wait_for_ > 0) {
		logger_.log_u(fz::logmsg::debug_warning, L"Still retrieving server's configuration");
		return;
	}

	wxPushDialog(this, wxID_ANY, _S("Export configuration"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER) | [this](wxDialog *p) {
		Checks *checks;

		wxVBox(p) = {
			wxWText(p, _F("Exporting configuration of server %s.", fz::to_wxString(server_info_.name))),

			wxStaticVBox(p, _S("Select parts to export:")) = checks = new Checks(p),
			wxEmptySpace,

			p->CreateButtonSizer(wxOK | wxCANCEL)
		};

		checks->SetValue(true);

		auto *ok = wxWindow::FindWindowById(wxID_OK, p);

		if (ok) {
			checks->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
				ev.Skip();

				ok->Enable(checks->IsAnyChecked());
			});
		}

		wxGUIEventLoop loop;
		wxValidate(p, [&]() mutable -> bool {
			if (loop.IsRunning())
				return false;

			if (!checks->IsAnyChecked()) {
				wxMsg::Error(_S("You must choose something to export."));
				return false;
			}

			if (ok) {
				ok->Disable();
				checks->Disable();
			}

			wxPushDialog<wxFileDialog>(p, _S("Choose a file where to export the configuration to."), wxEmptyString, wxEmptyString, wxT("*.xml"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT) | [&](wxFileDialog &d) {
				if (Settings()->working_dir().type() == fz::local_filesys::dir)
					d.SetDirectory(fz::to_wxString(Settings()->working_dir().str()));

				auto ret = d.ShowModal();

				Settings()->working_dir() = fz::to_native(d.GetDirectory());

				if (ok) {
					ok->Enable();
					checks->Enable();
				}

				if (ret == wxID_CANCEL)
					return loop.Exit(false);

				auto dest = d.GetPath();
				if (!dest.EndsWith(wxT(".xml")))
					dest += wxT(".xml");

				on_settings_received_func_ = [this
					, dest = fz::to_native(dest)
					, checks
					, &loop
				] {
					on_settings_received_func_ = nullptr;

					logger_.log_u(fz::logmsg::status, _S("Server's configuration retrieved. Exporting now..."));

					using namespace fz::serialization;

					xml_output_archive::file_saver saver(dest);	{
						xml_output_archive ar(saver, xml_output_archive::options().root_node_name("filezilla-server-exported"));

						if (checks->listeners_and_protocols) {
							ar(
								nvp(protocols_options_, "protocols_options"),
								nvp(ftp_options_, "ftp_options"),
								nvp(webui_options_, "webui_options"),
								nvp(disallowed_ips_, "disallowed_ips"),
								nvp(allowed_ips_, "allowed_ips")
							);
						}

						if (checks->rights_management) {
							ar(
								nvp(groups_, "groups"),
								nvp(users_, "users")
							);
						}

						if (checks->administration) {
							ar(nvp(admin_options_, "admin_options"));
						}

						if (checks->logging) {
							ar(nvp(logger_options_, "logger_options"));
						}

						if (checks->acme) {
							ar(
								nvp(acme_options_, "acme_options"),
								nvp(acme_extra_account_info_, "acme_extra_account_info")
							);
						}

						if (checks->pkcs11) {
							ar(
								nvp(pkcs11_options_, "pkcs11_options")
							);
						}

						#ifdef ENABLE_FZ_UPDATE_CHECKER
							if (checks->updates)
								ar(nvp(updates_options_, "updates_options"));
						#endif
					}

					if (!saver) {
						wxMsg::Error(_S("Failed to export configuration."));
						return loop.Exit(false);
					}

					logger_.log_u(fz::logmsg::status, _S("Server's configuration exported to %s."), dest);
					return loop.Exit(true);
				};

				logger_.log_raw(fz::logmsg::status, _S("Retrieving configuration from the server..."));

				responses_to_wait_for_ = 0;

				if (checks->listeners_and_protocols) {
					responses_to_wait_for_ += 3;
					client_.send<administration::get_ip_filters>();
					client_.send<administration::get_protocols_options>();
					client_.send<administration::get_ftp_options>(true);

					#ifdef ENABLE_FZ_WEBUI
						responses_to_wait_for_ += 1;
						client_.send<administration::get_webui_options>(true);
					#endif
				}

				if (checks->rights_management) {
					responses_to_wait_for_ += 1;
					client_.send<administration::get_groups_and_users>();
				}

				if (checks->administration) {
					responses_to_wait_for_ += 1;
					client_.send<administration::get_admin_options>(true);
				}

				if (checks->logging) {
					responses_to_wait_for_ += 1;
					client_.send<administration::get_logger_options>();
				}

				if (checks->acme) {
					responses_to_wait_for_ += 1;
					client_.send<administration::get_acme_options>();
				}

				if (checks->pkcs11) {
					responses_to_wait_for_ += 1;
					client_.send<administration::get_pkcs11_options>();
				}

				#ifdef ENABLE_FZ_UPDATE_CHECKER
					if (checks->updates) {
						responses_to_wait_for_ += 1;
						client_.send<administration::get_updates_options>();
					}
				#endif
			};

			return loop.Run();
		});

		p->ShowModal();
	};
}

void ServerAdministrator::ImportConfig()
{
	if (!IsConnected())
		return;

	wxPushDialog<wxFileDialog>(this, _S("Choose a file to import the configuration from."), wxEmptyString, wxEmptyString, wxT("*.xml"), wxFD_OPEN | wxFD_FILE_MUST_EXIST) | [this](wxFileDialog &d) {
		if (Settings()->working_dir().type() == fz::local_filesys::dir)
			d.SetDirectory(fz::to_wxString(Settings()->working_dir().str()));

		auto ret = d.ShowModal();

		Settings()->working_dir() = fz::to_native(d.GetDirectory());

		if (ret == wxID_CANCEL) {
			return;
		}

		auto src = fz::to_native(d.GetPath());
		config_parts cp;
		auto msgs = cp.load(src);
		if (!msgs) {
			return;
		}

		auto checks = new Checks(this);
		checks->Hide();

		checks->listeners_and_protocols.EnableAndSet(
			(cp.protocols_options || cp.ftp_options || cp.webui_options) &&
			cp.disallowed_ips && cp.allowed_ips
		);

		checks->rights_management.EnableAndSet(
			cp.groups && cp.users
		);

		checks->administration.EnableAndSet(
			cp.admin_options.has_value()
		);

		checks->logging.EnableAndSet(
			cp.logger_options.has_value()
		);

		checks->acme.EnableAndSet(
			cp.acme_options && cp.acme_extra_account_info
		);

		checks->acme.EnableAndSet(
			cp.pkcs11_options.has_value()
		);

		#ifdef ENABLE_FZ_UPDATE_CHECKER
			checks->updates.EnableAndSet(
				cp.updates_options.has_value()
			);
		#endif

		if (!checks->IsAnyChecked()) {
			wxMsg::Error(_S("Chosen file doesn't contain any useful data."));
			return;
		}

		wxGUIEventLoop loop;

		wxPushDialog(&d, wxID_ANY, _S("Import configuration"), wxDefaultPosition, wxDefaultSize, wxCAPTION | (wxRESIZE_BORDER * !msgs->empty())) | [&](wxDialog *p) {
			checks->Reparent(p);

			{
				auto vb = wxVBox(p) = {
					wxWText(p, _F("Importing configuration of server %s from file \"%s\".", fz::to_wxString(server_info_.name), src)),
					wxStaticVBox(p, _S("Select which configuration's parts to import:")) = checks |[&](auto p) { p->Show(); },
				};

				if (!msgs->empty()) {
					vb += {
						wxLabel(p, _S("Notes:")),
						wxSizerFlags(1).Expand() >>= new wxTextCtrl(p, wxID_ANY, *msgs, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY),
					};
				}

				vb += p->CreateButtonSizer(wxOK | wxCANCEL);
			}

			if (auto *ok = wxWindow::FindWindowById(wxID_OK, p)) {
				checks->Bind(wxEVT_CHECKBOX, [ok, checks](wxCommandEvent &ev) {
					ev.Skip();

					ok->Enable(checks->IsAnyChecked());
				});
			}

			wxValidate(p, [&] {
				if (!checks->IsAnyChecked()) {
					wxMsg::Error(_S("You must choose something to import."));
					return false;
				}

				return true;
			});

			if (p->ShowModal() == wxID_OK) {
				if (checks->listeners_and_protocols) {
					if (cp.disallowed_ips && cp.allowed_ips)
						client_.send<administration::set_ip_filters>(*cp.disallowed_ips, *cp.allowed_ips);

					if (cp.protocols_options)
						client_.send<administration::set_protocols_options>(*cp.protocols_options);

					if (cp.ftp_options)
						client_.send<administration::set_ftp_options>(*cp.ftp_options);

					#ifdef ENABLE_FZ_WEBUI
						if (cp.webui_options)
							client_.send<administration::set_webui_options>(*cp.webui_options);
					#endif
				}

				if (checks->rights_management) {
					if (cp.groups && cp.users)
						client_.send<administration::set_groups_and_users>(*cp.groups, *cp.users, true);
				}

				if (checks->administration) {
					if (cp.admin_options)
						client_.send<administration::set_admin_options>(*cp.admin_options);
				}

				if (checks->logging) {
					if (cp.logger_options)
						client_.send<administration::set_logger_options>(*cp.logger_options);
				}

				if (checks->acme) {
					if (cp.acme_options && cp.acme_extra_account_info) {
						client_.send<administration::restore_acme_account>(cp.acme_options->account_id, *cp.acme_extra_account_info);
						client_.send<administration::set_acme_options>(*cp.acme_options);
					}
				}

				if (checks->pkcs11) {
					if (cp.pkcs11_options) {
						client_.send<administration::set_pkcs11_options>(*cp.pkcs11_options);
					}
				}

				#ifdef ENABLE_FZ_UPDATE_CHECKER
					if (checks->updates) {
						if (cp.updates_options)
							client_.send<administration::set_updates_options>(*cp.updates_options);
					}
				#endif
			}

			loop.Exit();
		};

		loop.Run();
	};
}

std::optional<wxString> ServerAdministrator::config_parts::load(fz::native_string_view src)
{
	using namespace fz::serialization;

	xml_input_archive::file_loader loader{fz::native_string(src)};
	auto error = load_current(loader, xml_input_archive::options::verify_mode::error);

	if (auto mismatch = error.flavour_or_version_mismatch()) {
		int res = wxMsg::ErrorConfirm(_S("There was a problem while reading the configuration from file.")).Ext(_S(
			"%s\n\n"
			"The configuration can still be imported, but some data might be lost.\n\n"
			"Do you want to proceed anyway?"
		), fz::to_wxString(mismatch->description));

		if (res == wxID_YES) {
			error = load_current(loader, xml_input_archive::options::verify_mode::ignore);
		}
		else {
			return {};
		}
	}

	if (error.root_node_missing()) {
		error = load_ancient(loader);

		if (error.root_node_missing()) {
			wxMsg::Error(_S("Error while reading the configuration from file."))
				.Ext(_S("Unknown file format."), src);

			return {};
		}
	}

	if (auto other = error.other_error()) {
		wxMsg::Error(_S("Error while reading the configuration from file."))
			.Ext(fz::to_wxString(other->description));

		return {};
	}

	if (auto no_error = error.no_error()) {
		return {fz::to_wxString(no_error->messages)};
	}

	return {wxEmptyString};
}

ServerAdministrator::config_parts::loading_error ServerAdministrator::config_parts::load_current(fz::serialization::xml_input_archive::file_loader &loader, fz::serialization::xml_input_archive::options::verify_mode verify_mode)
{
	using namespace fz::serialization;

	xml_input_archive ar(loader, xml_input_archive::options()
		 .root_node_name("filezilla-server-exported")
		 .verify_version(verify_mode)
	);

	ar(
		nvp(protocols_options, "protocols_options"),
		nvp(ftp_options, "ftp_options"),
		nvp(webui_options, "webui_options"),
		nvp(disallowed_ips, "disallowed_ips"),
		nvp(allowed_ips, "allowed_ips"),

		nvp(groups, "groups"),
		nvp(users, "users"),

		nvp(admin_options, "admin_options"),

		nvp(logger_options, "logger_options"),

		nvp(acme_options, "acme_options"),
		nvp(acme_extra_account_info, "acme_extra_account_info"),

		nvp(pkcs11_options, "pkcs11_options")

	#ifdef ENABLE_FZ_UPDATE_CHECKER
			, nvp(updates_options, "updates_options")
	#endif
	);

	if (!ar) {
		if (ar.error().is_root_node_missing()) {
			return root_node_missing{};
		}

		if (ar.error().is_flavour_or_version_mismatch()) {
			return flavour_or_version_mismatch{ar.error().description()};
		}

		return other_error{ar.error().description()};
	}

	return {};
}

ServerAdministrator::config_parts::loading_error ServerAdministrator::config_parts::load_ancient(fz::serialization::xml_input_archive::file_loader &loader)
{
	using namespace fz::serialization;

	struct logger_to_string: fz::logger_interface
	{
		logger_to_string(std::string &str)
			: str_(str)
		{
			enable(fz::logmsg::warning);
		}

		void do_log(fz::logmsg::type t, std::wstring && msg) override
		{
			str_
				.append(fz::logger::type2str<std::string_view>(t, false))
				.append(" ")
				.append(fz::to_utf8(msg))
				.append("\n");
		}

	private:
		std::string &str_;
	};

	std::string msg;
	logger_to_string logger(msg);
	fz::configuration::old::server_config old_config;

	// Load up old configuration file
	{
		xml_input_archive ar(loader, xml_input_archive::options().root_node_name("FileZillaServer"));

		ar(nvp(old_config, ""));

		if (!ar) {
			if (ar.error().is_root_node_missing()) {
				return root_node_missing{};
			}

			return other_error{ar.error().description()};
		}
	}

	// Convert Old -> New
	server_settings new_settings;
	fz::authentication::file_based_authenticator::groups new_groups;
	fz::authentication::file_based_authenticator::users new_users;
	fz::tcp::binary_address_list new_disallowed_ips;
	fz::tcp::binary_address_list new_allowed_ips;

	fz::authentication::file_based_authenticator::groups::value_type *speed_limited_group{};

	fz::configuration::old::converter converter(old_config, logger);

	bool success =
		converter.extract(new_groups, &speed_limited_group) &&
		converter.extract(new_users, speed_limited_group) &&
		converter.extract(new_settings) &&
		converter.extract(new_disallowed_ips, new_allowed_ips);

	if (success) {
		protocols_options = std::move(new_settings.protocols);
		ftp_options = std::move(new_settings.ftp_server);
		admin_options = std::move(new_settings.admin);
		disallowed_ips = std::move(new_disallowed_ips);
		allowed_ips = std::move(new_allowed_ips);

		if (!new_groups.empty() || !new_users.empty()) {
			groups = std::move(new_groups);
			users = std::move(new_users);
		}

		return no_error{ std::move(msg) };
	}

	return other_error{ std::move(msg) };
}
