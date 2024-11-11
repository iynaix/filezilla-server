#include <libfilezilla/glue/wx.hpp>

#include <wx/sizer.h>
#include <wx/bookctrl.h>
#include <wx/treebook.h>
#include <wx/choicdlg.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/display.h>
#include <wx/choicebk.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/log.h>
#include <wx/statline.h>
#include <wx/valgen.h>
#include <wx/simplebook.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/treectrl.h>
#include <wx/compositewin.h>

#include "passwordeditor.hpp"
#include "certinfoeditor.hpp"

#include "settingsdialog.hpp"
#include "groupseditor.hpp"
#include "userseditor.hpp"
#include "helpers.hpp"
#include "textvalidatorex.hpp"

#include "addressinfolisteditor.hpp"
#include "passwordeditor.hpp"
#include "certinfoeditor.hpp"
#include "filterseditor.hpp"
#include "integraleditor.hpp"

#include "locale.hpp"
#include "glue.hpp"

#include "../filezilla/util/bits.hpp"
#include "../filezilla/logger/type.hpp"


enum {
	impersonator_msw,
	impersonator_nix
};

static wxString address_and_port_in_other_editor(wxString address, unsigned int port, wxString other_address, const wxString &editor)
{
	if (address.empty())
		address = _F("Local port %d", port);
	else
		address = _F("Address %s", fz::join_host_and_port(address.ToStdWstring(), port));

	if (other_address.empty())
		other_address = _F("local port %d", port);
	else
		other_address = _F("address %s", fz::join_host_and_port(other_address.ToStdWstring(), port));

	if (address == other_address)
		return _F("%s is already used by %s", address, editor);

	return _F("%s conflicts with %s in %s", address, other_address, editor);
}

SettingsDialog::SettingsDialog()
{}

bool SettingsDialog::Create(wxWindow *parent, const wxString &server_name, fz::util::fs::path_format server_path_format, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	auto title = _F("Settings for server %s", server_name);

	if (!wxDialogEx::Create(parent, winid, title, pos, size, style, name))
		return false;

	server_path_format_ = server_path_format;
	server_name_ = fz::to_utf8(server_name);

	Bind(wxEVT_SHOW, [&](wxShowEvent &ev) {
		if (!ev.IsShown())
			return;

		CallAfter([&] {
			static_cast<wxTreebook*>(GetBookCtrl())->GetTreeCtrl()->SetFocusFromKbd();
		});

		check_expired_certs();
	});

	auto make_security_editor = [&](wxChoice *&min_tls_ver_ctrl, CertInfoEditor *&cert_info_ctrl) {
		return [&](auto p) {
			return wxVBox(p) = {
				{ 0, wxHBox(p, 0) = {
					{ 0, wxLabel(p, _S("Mini&mum allowed TLS version:")) },
					{ 1, min_tls_ver_ctrl = new wxChoice(p, wxID_ANY)  | [&](wxChoice * p) {
						p->Append(_S("v1.2"));
						p->Append(_S("v1.3"));

						p->SetSelection(0);
					}},
				}},

				{ 0, new wxStaticLine(p, wxID_ANY) },

				{ 1, cert_info_ctrl = wxCreate<CertInfoEditor>(p) }
			};
		};
	};

	static_cast<wxTreebook *>(GetBookCtrl()) | [&](auto p) {
		wxPage(p, _S("Server listeners")) | [&](auto p) {
			wxVBox(p, 0) = wxSizerFlags(1).Expand() >>= server_listeners_editor_ = wxCreate<AddressInfoListEditor>(p);
		};

		wxPage(p, _S("Protocols settings")) | [&](auto p) {
			wxVBox(p, 0) = wxCreate<wxNotebook>(p, wxID_ANY) | [&](auto p) {
				wxPage(p, _S("Autoban")) | [&](auto p) {
					wxVBox(p) = {
						{ 0, wxLabel(p, _S("&Ban IP address after:")) },
						{ 0, autoban_max_login_failures_ctrl_ = wxCreate<IntegralEditor>(p, _S("failed login attempts (0 disables the functionality)."), 1) },

						{ 0, wxLabel(p, _S("If they &happen within:")) },
						{ 0, autoban_login_failures_time_window_ctrl_ = wxCreate<IntegralEditor>(p, _S("seconds."), 1000) },

						{ 0, wxLabel(p, _S("The ban will &last:")) },
						{ 0, autoban_ban_duration_ctrl_ = wxCreate<IntegralEditor>(p, _S("seconds."), 1000) }
					};
				};

				wxPage(p,  _S("Timeouts")) | [&](auto p) {
					wxVBox(p) = {
						{ 0, wxLabel(p, _S("&Login timeout:")) },
						{ 0, login_timeout_ctrl_ = wxCreate<IntegralEditor>(p, _S("seconds."), 1000) },

						{ 0, wxLabel(p, _S("Acti&vity timeout:")) },
						{ 0, activity_timeout_ctrl_ = wxCreate<IntegralEditor>(p, _S("minutes."), 60000) },
					};
				};

				wxPage(p, _S("Performance")) | [&](auto p) {
					wxVBox(p) = {
						wxLabel(p, _S("Number of &threads:")),
						performance_number_of_session_threads_ctrl_ = wxCreate<IntegralEditor>(p),

						server_receiving_buffer_size_check_ = new wxCheckBox(p, wxID_ANY, _S("Customize TCP recei&ve buffer size for data socket:")),
						performance_receiving_buffer_size_ctrl_ = wxCreate<IntegralEditor>(p, _S("KiB"), 1024),

						server_sending_buffer_size_check_ = new wxCheckBox(p, wxID_ANY, _S("Customize TCP se&nd buffer size for data socket:")),
						performance_sending_buffer_size_ctrl_ = wxCreate<IntegralEditor>(p, _S("KiB"), 1024)
					};

					server_receiving_buffer_size_check_->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
						performance_receiving_buffer_size_ctrl_->SetValue(ev.GetInt() ? 0 : -1);
					});

					server_sending_buffer_size_check_->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
						performance_sending_buffer_size_ctrl_->SetValue(ev.GetInt() ? 0 : -1);
					});

					performance_receiving_buffer_size_ctrl_->Bind(IntegralEditor::Event::Changed, [&](IntegralEditor::Event &) {
						decltype(protocols_options_.performance.receive_buffer_size) size;

						bool enabled = performance_receiving_buffer_size_ctrl_->Get(size) && size != -1;

						performance_receiving_buffer_size_ctrl_->Enable(enabled);
						server_receiving_buffer_size_check_->SetValue(enabled);
					});

					performance_sending_buffer_size_ctrl_->Bind(IntegralEditor::Event::Changed, [&](IntegralEditor::Event &) {
						decltype(protocols_options_.performance.send_buffer_size) size;

						bool enabled = performance_sending_buffer_size_ctrl_->Get(size) && size != -1;

						performance_sending_buffer_size_ctrl_->Enable(enabled);
						server_sending_buffer_size_check_->SetValue(enabled);
					});
				};

				wxPage(p, _S("Filters")) |[&](auto p) {
					wxVBox(p) = filters_ctrl_ = wxCreate<FiltersEditor>(p);
				};
			};

			wxPage(p, _S("FTP and FTP over TLS (FTPS)")) | [&](auto p) {
				wxVBox(p, 0) = wxCreate<wxNotebook>(p, wxID_ANY) | [&](auto p) {
					wxPage(p, _S("Connection Security")) | make_security_editor(ftp_tls_min_ver_ctrl_, ftp_cert_info_ctrl_);

					wxPage(p, _S("Welcome message")) | [&](auto p) {
						wxVBox(p) = {
							{ 0, wxLabel(p, _S("Custom &welcome message:"))},
							{ 1, welcome_message_ctrl_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE) },
						};

						auto font = welcome_message_ctrl_->GetFont();
						font.SetFamily(wxFONTFAMILY_TELETYPE);
						welcome_message_ctrl_->SetFont(font);
					};

					pasv_page_ = wxPage(p, _S("Passive mode")) | [&](auto p) {
						wxVBox(p) = {
							use_custom_port_range_ctrl_ = new wxCheckBox(p, nullID, _S("&Use custom port range:")),
							wxVBox(p, {wxLEFT, wxDefaultPadding*2}) = {
								wxLabel(p, _F("&From: (suggested is %d)", fz::port_randomizer::min_ephemeral_value)),
								min_port_range_ctrl_ = wxCreate<IntegralEditor>(p),

								wxLabel(p, _F("&To: (suggested is %d)", fz::port_randomizer::max_ephemeral_value)),
								max_port_range_ctrl_ = wxCreate<IntegralEditor>(p)
							},

							wxLabel(p, _S("Public IP or &hostname: (leave it empty to use the local IP)")),
							wxHBox(p, 0) = {
								wxSizerFlags(1) >>= host_override_ctrl_ = new wxTextCtrl(p, nullID),
								fx::RetrievePublicIpButton(p, *host_override_ctrl_, fx::ipv_type::ipv4, get_public_ip_func_)
							},
							disallow_host_override_for_local_peers_ctrl_ = new wxCheckBox(p, nullID, _S("Use local IP for &local connections (recommended).")),
						};

						wxTransferDataFromWindow(p, [&] {
							const wxString &val = host_override_ctrl_->GetValue();
							bool is_valid = !host_override_ctrl_->IsModified() || fx::ValidatePassiveModeHostMsg(val);

							if (is_valid) {
								host_override_ctrl_->SetModified(false);
								ftp_options_.sessions().pasv.host_override = fz::to_native(val);
							}
							else {
								host_override_ctrl_->SetFocusFromKbd();
							}

							return is_valid;
						});

						use_custom_port_range_ctrl_->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent &ev) {
							bool enabled = ev.GetInt();

							auto &range = ftp_options_.sessions().pasv.port_range;

							if (enabled) {
								range.emplace();
								min_port_range_ctrl_->Enable();
								max_port_range_ctrl_->Enable();
								min_port_range_ctrl_->SetRef(range->min, 1, 65535);
								max_port_range_ctrl_->SetRef(range->max, 1, 65535);
							}
							else {
								range.reset();
								min_port_range_ctrl_->Disable();
								max_port_range_ctrl_->Disable();
								min_port_range_ctrl_->SetRef(nullptr);
								max_port_range_ctrl_->SetRef(nullptr);
							}
						});

						wxTransferDataFromWindow(p, [this] {
							if (auto &range = ftp_options_.sessions().pasv.port_range) {
								if (range->max < range->min) {
									std::swap(range->max, range->min);
									min_port_range_ctrl_->SetRef(range->min, 1, 65535);
									max_port_range_ctrl_->SetRef(range->max, 1, 65535);
								}
							}

							return true;
						});
					};
				};
			};

		#ifdef ENABLE_FZ_WEBUI
			wxPage(p, _S("WebUI")) | [&](auto p) {
				wxVBox(p, 0) = wxCreate<wxNotebook>(p, wxID_ANY) | [&](auto p) {
					wxPage(p, _S("Connection Security")) | make_security_editor(webui_tls_min_ver_ctrl_, webui_cert_info_ctrl_);

					wxPage(p, _S("Tokens Management")) | [&](auto p) {
						wxButton *destroy{};
						wxButton *enable_refreshing{};

						wxVBox(p) = {
							wxGBox(p, 2, {1}) = {
								wxLabel(p, _S("&Access token timeout (minutes):")),
								webui_access_token_timeout_ctrl_ = wxCreate<IntegralEditor>(p, wxEmptyString, fz::duration::from_minutes(1).get_milliseconds()),

								wxLabel(p, _S("&Refresh token timeout (days):")),
								webui_refresh_token_timeout_ctrl_ = wxCreate<IntegralEditor>(p, wxEmptyString, fz::duration::from_days(1).get_milliseconds()),

							},

							new wxStaticLine(p),

							destroy = new wxButton(p, nullID, _S("Revoke all existing tokens.")),
							enable_refreshing = new wxButton(p, nullID, _S("Enable token refreshing for all existing users."))
						};

						destroy->Bind(wxEVT_BUTTON, [this](auto &) {
							wxCHECK_RET(webui_destroy_tokens_func_ != nullptr, _S("The webui_destroy_tokens_func_ is NULL"));

							if (!webui_destroy_tokens_func_) {
								return;
							}

							auto err = webui_destroy_tokens_func_();
							if (err.empty()) {
								wxMsg::Success(_S("Tokens successfully revoked"));
							}
							else {
								wxMsg::Error(_S("Couldn't revoke the tokens")).Ext(wxT("%s"), err).Wait();
							}
						});

						enable_refreshing->Bind(wxEVT_BUTTON, [this](auto &) {
							auto set = fz::authentication::methods_set();
							set.add<fz::authentication::method::token>();

							for (auto &[name, u]: users_) {
								if (!u.methods.has(set)) {
									u.methods.push_back(set);
								}
							}

							wxMsg::Success(_S("All users can now refresh their tokens."));
						});
					};
				};
			};
		#endif
		};


		authentication_page_ = wxPage(p, _S("Rights management")) | [&](auto p) {
			wxRadioButton *radio_default{}, *radio_other{};
			wxPanel *panel_default{}, *panel_other{};
			wxWindow *label_default{};

			wxStaticVBox(p, _S("Default system user for filesystem access"), 0) = wxVBox(p) = {
				wxWText(p, _S("This setting controls which system user is used by the server for accessing files and directories. It applies to all server users that are not configured to use their own system credentials for accessing files and directories.")),
				wxWText(p, _S("The filesystem permissions of the system user selected here are checked for all file and directory operations. Newly created files and directories will have this system user listed as owner.")),

				radio_default = new wxRadioButton(p, 0, _S("Use system &user the server is running under")),
				panel_default = new wxPanel(p) | [&] (auto p) {
					wxVBox(p) =	label_default = wxWText(p, _F("The server you are connected to is running as %s.", server_username_));
				},

				radio_other = new wxRadioButton(p, 1, _S("Use &other system user")),
				panel_other = new wxPanel(p) | [&] (auto p) {
					wxVBox(p) =	default_impersonator_book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(p) | [&](auto p) {
						wxPage(wxValidateOnlyIfCurrent)(p, wxS("*MSW*")) |[&](auto p) {
							wxVBox(p, 0) = {
								{ 0, wxLabel(p, _S("Name:")) },
								{ 0, default_impersonator_name_msw_ = new wxTextCtrl(p, wxID_ANY) },

								{ 0, wxLabel(p, _S("Password:")) },
								{ 0, default_impersonator_password_msw_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD) },
							};
						};

						wxPage(wxValidateOnlyIfCurrent)(p, wxS("*NIX*")) |[&](auto p) {
							wxVBox(p, 0) = {
								{ 0, wxLabel(p, _S("Name:")) },
								{ 0, default_impersonator_name_nix_ = new wxTextCtrl(p, wxID_ANY) },

								{ 0, wxLabel(p, _S("Group (optional, leave blank to pick the default one):")) },
								{ 0, default_impersonator_group_nix_ = new wxTextCtrl(p, wxID_ANY) },
							};
						};
					};
				},

				wxWText(p, _S("Access rights for all server users are further restricted by their configured mounts and permissions."))
			};

			auto set_selected = [v = std::vector<std::pair<wxRadioButton *, wxPanel *>>{{radio_default, panel_default}, {radio_other, panel_other}}](std::size_t n) {
				if (n >= v.size())
					return;

				for (auto i = std::size_t{}, end_i = v.size(); i < end_i;  ++i) {
					auto enabled = i == n;
					v[i].first->SetValue(enabled);
					v[i].second->Enable(enabled);
				}
			};

			p->Bind(wxEVT_RADIOBUTTON, [set_selected](wxCommandEvent &evt) {
				set_selected(std::size_t(evt.GetId()));
			});

			wxTransferDataToWindow(p, [this, p, label_default, radio_other, set_selected = std::move(set_selected)] {
				bool can_impersonate = can_impersonate_;

				if (auto imp = users_.default_impersonator.msw()) {
					default_impersonator_book_->ChangeSelection(impersonator_msw);

					radio_other->SetValidator(wxGenericValidator(&imp->enabled));
					set_selected(imp->enabled);

					default_impersonator_name_msw_->SetValidator(TextValidatorEx(wxFILTER_NONE, &imp->name, field_must_not_be_empty(_S("Name"))));
					default_impersonator_password_msw_->SetValidator(TextValidatorEx(wxFILTER_NONE, &imp->password, field_must_not_be_empty(_S("Password"))));
				}
				else
				if (auto imp = users_.default_impersonator.nix()) {
					default_impersonator_book_->ChangeSelection(impersonator_nix);

					radio_other->SetValidator(wxGenericValidator(&imp->enabled));
					set_selected(imp->enabled);

					default_impersonator_name_nix_->SetValidator(TextValidatorEx(wxFILTER_NONE, &imp->name, field_must_not_be_empty(_S("Name"))));
					default_impersonator_group_nix_->SetValidator(TextValidatorEx(wxFILTER_NONE, &imp->group));
				}
				else
					can_impersonate = false;

				label_default->SetLabel(_F("The server you are connected to is running as %s.", server_username_));
				label_default->Refresh();

				p->Enable(can_impersonate);
				return true;
			});

			groups_editor_ = wxPage<GroupsEditor>(p, _S("Groups"));

			users_editor_ = wxPage<UsersEditor>(p, _S("Users"));
		};

		wxPage(wxValidateOnlyIfCurrentOrWhenLeaving)(p, _S("Administration"), false, wxID_ANY) | [&](auto p) {
			wxVBox(p, 0) = wxCreate<wxNotebook>(p, wxID_ANY) | [&](auto p) {
				wxPage(wxValidateOnlyIfCurrentOrWhenLeaving)(p,  _S("Connection")) | [&](auto p) {
					#if defined ENABLE_ADMIN_DEBUG && ENABLE_ADMIN_DEBUG
						bool const edit_tls_mode = true;
					#else
						bool const edit_tls_mode = false;
					#endif

					wxVBox(p) = {
						wxLabel(p, _S("&Local listening port:")),
						admin_port_ctrl_ = wxCreate<IntegralEditor>(p),

						wxLabel(p, _S("&Password:")),
						admin_password_ctrl_ = wxCreate<PasswordEditor>(p),

						wxLabel(p, _S("Additional lis&teners (only available if a password is set):")),
						wxSizerFlags(1) >>= admin_listeners_editor_ = wxCreate<AddressInfoListEditor>(p, edit_tls_mode)
					};
				};

				wxPage(wxValidateOnlyIfCurrentOrWhenLeaving)(p, _S("Connection security")) | [&](auto p) {
					wxVBox(p) = admin_cert_info_ctrl_ = wxCreate<CertInfoEditor>(p);
				};
			};

			admin_password_ctrl_->Bind(PasswordEditor::Event::Changed, [&](PasswordEditor::Event &) {
				admin_listeners_editor_->Enable(admin_password_ctrl_->HasPassword());
			});

			admin_listeners_editor_->Disable();
		};

		wxPage(p, _S("Logging")) | [&](auto p) {
			wxVBox(p, 0) = {
				{ 0, wxLabel(p,  _S("Logging &level (the higher the more verbose):")) },
				{ 0, log_level_ctrl_ = new wxChoice(p, wxID_ANY) | [&](wxChoice * p) {
					p->Append(_S("0 - None"));
					p->Append(_S("1 - Normal"));
					p->Append(_S("2 - Warning"));
					p->Append(_S("3 - Info"));
					p->Append(_S("4 - Verbose"));
					p->Append(_S("5 - Debug"));

					p->SetSelection(0);
				}},

				{ 0, wxLabel(p,  _S("Log &output:")) },
				{ 0, logging_choice_ctrl_ = new wxChoicebook(p, wxID_ANY) |[&](wxChoicebook *p) {
					wxPage(wxValidateOnlyIfCurrent)(p, _S("To file")) |[&](auto p) {
						wxVBox(p, 0) = {
							wxLabel(p, _S("Log &file name, including absolute path:")),
							log_path_ctrl_ = new wxTextCtrl(p, wxID_ANY),

							log_rotation_choice_ctrl_ = new wxChoicebook(p, wxID_ANY) |[&](wxChoicebook *p) {
								wxPage(wxValidateOnlyIfCurrent)(p, _S("File rotation is disabled")) | [&](auto p) {
									wxTransferDataFromWindow(p, [this] {
										logger_opts_.max_amount_of_rotated_files(0);
										return true;
									});
								};

								wxPage(wxValidateOnlyIfCurrent)(p, _S("File rotation is enabled")) | [&](auto p) {
									wxVBox(p, 0) = {
										wxLabel(p, _S("Maximum number of &log files:")),
										log_rotations_amount_ctrl_ = wxCreate<IntegralEditor>(p),

										wxLabel(p, _S("Log rotation pol&icy:")),
										log_rotation_type_ctrl_ = new wxChoicebook(p, wxID_ANY) |[&](wxChoicebook *p) {
											wxPage(wxValidateOnlyIfCurrent)(p, _S("Rotate log file when its size reaches the maximum size")) | [&](auto p) {
												wxVBox(p, 0) = {
													wxLabel(p, _S("Maximum file si&ze:")),
													log_file_size_ctrl_ = wxCreate<IntegralEditor>(p, _S("MiB"), 1024*1024)
												};
											};

											wxPage(wxValidateOnlyIfCurrent)(p, _S("Rotate log files daily")) | [&](auto p) {
												wxVBox(p, 0) = wxSizerFlags(0).Top() >>= log_date_in_name_ = wxCreate<wxCheckBox>(p, wxID_ANY, _S("Append the &date of the file to its name when rotating, before any suffix."));
											};

											wxTransferDataFromWindow(p, [p, this] {
												logger_opts_.rotation_type(fz::logger::file::rotation_type(p->GetSelection()));
												return true;
											});
										}
									};
								};
							},
						};

						wxTransferDataFromWindow(p, [&] {
							logger_opts_.include_headers(true);
							return true;
						});
					};

					wxPage(wxValidateOnlyIfCurrent)(p, _S("To standard error"), true) |[&](auto p) {
						log_include_headers_ctrl_ = new wxCheckBox(p, wxID_ANY, _S("Log lines are timestamped."));

						wxTransferDataFromWindow(p, [&] {
							logger_opts_.name().clear();
							return true;
						});
					};

					p->Disable();
				}}
			};
		};

		auto acme_page = wxPage(p, _S("Let's Encrypt®")) | [&](auto p) {
			wxPanel *acme_panel{};

			wxVBox(p, 0) = {
				{ 0, acme_enabled_ = new wxCheckBox(p, wxID_ANY, _S("&Enable Let's Encrypt® certificate generation")) },
				{ 1, acme_panel = new wxPanel(p) | [&] (auto p) {
					wxVBox(p, 0) = {
						{ 0, wxStaticVBox(p, _S("Default account")) = wxGBox(p, 2, {1}) = {
							{ 0, wxLabel(p,  _S("ID:")) },
							{ 0, acme_account_id_ctrl_ = new wxTextCtrl(p, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY) },

							{ 0, wxLabel(p,  _S("Directory:")) },
							{ 0, new wxPanel(p) | [&](auto p) {
								wxVBox(p, 0) = acme_account_directory_ctrl_ = new wxTextCtrl(p, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
								wxTransferDataFromWindow(p, [this] {
									if (acme_account_directory_ctrl_->GetValue().IsEmpty() && acme_enabled_->GetValue()) {
										wxMsg::Error(_S("You must create an account, or disable Let's Encrypt® certificate generation."));
										return false;
									}

									return true;
								});
							}},
							{ 0, wxLabel(p,  _S("Contacts:")) },
							{ 0, acme_account_contacts_ctrl_ = new wxTextCtrl(p, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY) }
						}},
						{ 0, acme_account_id_button_ = new wxButton(p, wxID_ANY, _S("Create new account")) },

						{ 0, wxWText(p,  _S("&How to perform the challenges:")) },
						{ 1, acme_how_ctrl_ = new wxChoicebook(p, wxID_ANY) | [&](wxChoicebook * p) {
							wxPage(wxValidateOnlyIfCurrent)(p, _S("Please, make a choice")) |[&](auto p) {
								wxTransferDataFromWindow(p, [this] {
									if (acme_enabled_->GetValue()) {
										wxMsg::Error(_S("You must choose how to perform a challenge"));
										return false;
									}

									acme_opts_.how_to_serve_challenges = {};
									return true;
								});
							};

							wxPage(wxValidateOnlyIfCurrent)(p, _S("Use an internal web server")) |[&] (auto p) {
								wxVBox(p, 0) = {
									{0, wxWText(p, _S("&Listeners:")) },
									{1, acme_listeners_ctrl_ = wxCreate<AddressInfoListEditor>(p, false) },
								};

								wxTransferDataFromWindow(p, [this] {
									if (acme_listeners_.empty()) {
										wxMsg::Error(_S("You must add at least one listener"));
										acme_listeners_ctrl_->SetFocus();
										return false;
									}

									acme_opts_.how_to_serve_challenges = fz::acme::serve_challenges::internally{acme_listeners_};

									return true;
								});

								wxTransferDataToWindow(p, [this] {
									if (!acme_listeners_.empty())
										return true;

									if (std::holds_alternative<fz::acme::serve_challenges::internally>(acme_opts_.how_to_serve_challenges))
										return true;

									acme_listeners_.push_back({"0.0.0.0", 80});
									acme_listeners_.push_back({"::", 80});
									acme_listeners_ctrl_->SetLists({
										{ acme_listeners_, _S("HTTP") }
									});

									return true;
								});
							};

							wxPage(wxValidateOnlyIfCurrent)(p, _S("Use an external web server")) |[&] (auto p) {
								wxVBox(p, 0) = {
									{ 0, wxLabel(p,  _S("&Path from which the web server responds to GET requests to \"/.well-known/acme-challenge/\":")) },
									{ 0, acme_well_knon_path_ctrl_ = new wxTextCtrl(p, wxID_ANY) },

									{ 0, acme_create_path_if_not_exist_ctrl_ = new wxCheckBox(p, wxID_ANY, _S("&Create path if it doesn't exist already.")) },
								};

								wxTransferDataFromWindow(p, [this] {
									acme_opts_.how_to_serve_challenges = fz::acme::serve_challenges::externally {
										acme_well_known_path_,
										acme_create_path_if_not_exist_ctrl_->GetValue()
									};

									return true;
								});
							};

							wxTransferDataFromWindow(p, [this] {
								ftp_cert_info_ctrl_->SetAcmeOptions(acme_opts_);
								//admin_cert_info_ctrl_->SetAcmeOptions(acme_opts_);

								#ifdef ENABLE_FZ_WEBUI
									webui_cert_info_ctrl_->SetAcmeOptions(acme_opts_);
								#endif

								return true;
							});
						}},

						{ 0, new wxStaticLine(p, wxID_ANY) },
						{ 0, wxLabel(p, _S("Let's Encrypt® is a trademark of the Internet Security Research Group. All rights reserved.")) |[&](wxWindow *p) {
							p->SetFont(p->GetFont().MakeItalic().MakeSmaller());
						}}
					};
				}
			}};

			acme_account_id_button_->Bind(wxEVT_BUTTON, [this](auto) mutable {
				if (acme_generate_account_func_) {
					acme_account_id_button_->Disable();

					acme_account_id_ctrl_->SetValue(_S("Generating..."));
					acme_account_directory_ctrl_->SetValue(_S("Generating..."));
					acme_account_contacts_ctrl_->SetValue(_S("Generating..."));
					acme_account_id_button_->SetLabel(_S("Generating..."));

					acme_account_id_ctrl_->Disable();
					acme_account_directory_ctrl_->Disable();
					acme_account_contacts_ctrl_->Disable();
					acme_account_id_button_->Disable();

					acme_generate_account_func_();
				}
			});

			acme_enabled_->Bind(wxEVT_CHECKBOX, [this, acme_panel] (const wxCommandEvent &ev) {
				if (ev.GetInt()) {
					acme_panel->Enable();
				}
				else {
					acme_panel->Disable();
					acme_how_ctrl_->ChangeSelection(0);
				}
			});
		};

		wxPage(wxValidateOnlyIfCurrent)(p, _S("PKCS#11")) | [&](auto p) {
			wxRadioButton *use_system_config{};
			wxRadioButton *customize_list{};
			wxTextCtrl *list{};
			wxWindow *note{};

			wxVBox(p, 0) = {
				use_system_config = new wxRadioButton(p, nullID, _S("Use the system's PKCS#11 configuration."),  wxDefaultPosition, wxDefaultSize, wxRB_GROUP),
				customize_list = new wxRadioButton(p, nullID, _S("Customize the list of PKCS#11 providers: (one for each line)")),
				wxSizerFlags(1).Expand() >>= wxVBox(p, {wxLEFT, wxDefaultPadding*2}) = {
					note = wxWText(p, _S("Note: for changes to the list of PKCS#11 providers to have effect, the server must be restarted.")),
					wxSizerFlags(1).Expand() >>= list = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE),
				}
			};

			use_system_config->Bind(wxEVT_RADIOBUTTON, [list, note](auto) {
				list->Disable();
				note->Disable();
			});

			customize_list->Bind(wxEVT_RADIOBUTTON, [list, note](auto) {
				list->Enable();
				note->Enable();
			});

			wxTransferDataToWindow(p, [list, note, use_system_config, customize_list, this] {
				if (pkcs11_options_.providers.empty()) {
					use_system_config->SetValue(true);

					list->Disable();
					note->Disable();
				}
				else {
					list->SetValue(fz::to_wxString(fz::join(pkcs11_options_.providers, fzT("\n"))));
					customize_list->SetValue(true);

					list->Enable();
					note->Enable();
				}

				return true;
			});

			wxTransferDataFromWindow(p, [list, this] {
				if (list->IsEnabled()) {
					if (list->GetValue().IsEmpty()) {
						wxMsg::Error(_S("The list of providers cannot be empty.")).Wait();
						return false;
					}

					auto providers = fz::strtok(fz::to_native(list->GetValue()), fzT("\r\n"));

					for (auto &path: providers) {
						if (auto res = fz::tvfs::validation::validate_native_path(path, server_path_format_); !res) {
							wxString what = fz::to_wxString(path);
							if (what.size() > 66) {
								what = what.substr(0, 65) + wxT("\u2026"); // \u2026 is the horizontal ellipsis
							}

							InvalidPathExplanation exp(res, server_path_format_, false, _F("path '%s'", what));
							wxMsg::Error(exp.main).Ext(exp.extra).Wait();
							return false;
						}
					}

					pkcs11_options_.providers = providers;
				}
				else {
					pkcs11_options_.providers.clear();
				}

				return true;
			});
		};

#if !defined(WITHOUT_FZ_UPDATE_CHECKER)
		wxPage(wxValidateOnlyIfCurrent)(p, _S("Updates")) | [&](auto p) {
			wxChoicebook *allowed_type;
			wxChoicebook *frequency;
			wxTextCtrl *callback_path;
			wxTextCtrl *current_version, *next_version, *last_update, *next_update;

			p->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES|wxWS_EX_VALIDATE_RECURSIVELY);

			wxVBox(p, 0) = {
				wxLabel(p, _F("Check for %s updates auto&matically:", fz::build_info::package_name)),
				wxCreate<wxNavigationEnabled<wxSimplebook>>(p) |[&](wxSimplebook *p) {
					p->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES|wxWS_EX_VALIDATE_RECURSIVELY);

					wxPage<wxChoicebook>(p, wxT("**EOL**"), false, wxID_ANY) |[&](wxChoicebook *p) {
						wxPage(p, _S("Disabled")) |[&](auto p) {
							wxVBox(p, wxLEFT) = wxLabel(p, _S("Automatic updates are disabled because of reached End Of Life."));
						};
					};

					frequency = wxPage<wxChoicebook>(p, wxT("**REGULAR**"), false, wxID_ANY) |[&](wxChoicebook *p) {
						wxPage(p, _S("Never")) | [&](auto p) {
							wxVBox(p, wxLEFT) = wxLabel(p, _S("Only manual update checks will be available."));
						};

						wxPage(p, _S("Once a week")) | [&](auto p) {
							wxVBox(p, wxLEFT) = wxLabel(p, _S("It's not possible to check for updates less frequently than once a week."));
						};

						wxPage(wxValidateOnlyIfCurrent)(p, _S("With a custom frequency")) | [&](auto p) {
							IntegralEditor *custom_days;

							wxVBox(p) = custom_days = wxCreate<IntegralEditor>(p, _S("days"), fz::duration::from_days(1).get_milliseconds());

							custom_days->SetRef(updates_options_.frequency(), fz::duration::from_days(8), fz::duration::from_days(30));
						};
					};

					p->Bind(wxEVT_UPDATE_UI, [this, p](wxUpdateUIEvent &ev) {
						ev.Skip();

						if (update_info_.is_eol())
							p->SetSelection(0);
						else
							p->SetSelection(1);
					});
				},

				wxLabel(p, _S("&When checking for updates, check for:")),
				allowed_type = wxCreate<wxChoicebook>(p, wxID_ANY) |[&](wxChoicebook *p) {
					wxPage(p, _S("Stable versions only"));
					wxPage(p, _S("Stable and beta versions"));
				},

				wxVBox(p, wxLEFT) = {
					wxWText(p, _S("Advice: unless you want to test new features, please keep using stable "
								  "versions only. Beta versions are development versions "
								  "meant for testing purposes. Use beta versions at your own risk."))
				},

				wxWText(p, _S("Path to program to be &invoked when updates are available (version number in the first parameter):")),
				callback_path = new wxTextCtrl(p, wxID_ANY),

				wxStaticVBox(p, _S("Update information")) = wxGBox(p, 2, {1}) = {
					wxLabel(p, _S("Current version:")),          current_version = new wxTextCtrl(p, wxID_ANY, fz::to_wxString(fz::to_string(fz::build_info::version)), wxDefaultPosition, wxDefaultSize, wxTE_READONLY),
					wxLabel(p, _S("Latest available version:")), next_version = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),
					wxLabel(p, _S("Date of last check:")),       last_update = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),
					wxLabel(p, _S("Date of next check:")),       next_update = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY)
				},

				update_check_button_ = new wxButton(p, wxID_ANY, _S("Perform check &now")),

				wxEmptySpace,
				new wxStaticLine(p, wxID_ANY),
				wxWText(p, _S("Update Check Privacy: in order to keep your product up-to-date, we transmit only the program version, OS, and CPU information. "
							  "Rest assured that any non-essential information is not transmitted, prioritizing your privacy and minimizing data transfer.")).Smaller().Italic()
			};

			callback_path->SetValidator(TextValidatorEx(wxFILTER_NONE, &updates_options_.callback_path()));

			update_check_button_->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
				if (update_check_func_ && update_check_func_()) {
						update_check_button_->Disable();
				}
			});

			p->Bind(wxEVT_UPDATE_UI, [this, next_version, last_update, next_update](wxUpdateUIEvent &ev) {
				ev.Skip();

					if (!update_info_)
						next_version->SetValue(_S("Not known"));
					else
					if (update_info_->version)
						next_version->SetValue(fz::to_wxString(fz::to_string(update_info_->version)));
					else
						next_version->SetValue(_S("EOL: no updates available"));

					if (update_last_check_)
						last_update->SetValue(fz::to_wxString(update_last_check_));
					else
						last_update->SetValue(_S("Not known"));

					if (update_info_.is_eol())
						next_update->SetValue(_S("EOL: no more automatic checks."));
					else
					if (update_next_check_)
						next_update->SetValue(fz::to_wxString(update_next_check_));
					else
						next_update->SetValue(_S("Not available"));

					if (!update_check_func_)
						update_check_button_->Disable();
			});

			wxTransferDataToWindow(p, [this, frequency] {
				if (!updates_options_.frequency())
					frequency->SetSelection(0);
				else
				if (updates_options_.frequency() <= fz::duration::from_days(7))
					frequency->SetSelection(1);
				else
					frequency->SetSelection(2);

				return true;
			});

			wxTransferDataFromWindow(p, [this, frequency] {
				if (frequency->GetSelection() == 0)
					updates_options_.frequency() = {};
				else
				if (frequency->GetSelection() == 1)
					updates_options_.frequency() = fz::duration::from_days(7);

				return true;
			});

			wxTransferDataToWindow(p, [this, allowed_type] {
				if (updates_options_.allowed_type() == fz::update::allow::beta)
					allowed_type->SetSelection(1);
				else
					allowed_type->SetSelection(0);

				return true;
			});

			wxTransferDataFromWindow(p, [this, allowed_type] {
				if (allowed_type->GetSelection() == 1)
					updates_options_.allowed_type() = fz::update::allow::beta;
				else
					updates_options_.allowed_type() = fz::update::allow::release;

				return true;
			});
		};
#endif

		groups_editor_->Bind(GroupsList::Event::Changed, [this] (GroupsList::Event &) {
			users_editor_->SetGroups(groups_);
		});

		groups_editor_->Bind(GroupsList::Event::Changing, &SettingsDialog::OnGroupsListChanging, this);

		ftp_cert_info_ctrl_->SetSwitchToAcmeOptsFunc([p, pos = acme_page.GetPagePos()] {
			p->ChangeSelection(size_t(pos));
		});

		#ifdef ENABLE_FZ_WEBUI
			webui_cert_info_ctrl_->SetSwitchToAcmeOptsFunc([p, pos = acme_page.GetPagePos()] {
				p->ChangeSelection(size_t(pos));
			});
		#endif

		admin_cert_info_ctrl_->SetSwitchToAcmeOptsFunc([p, pos = acme_page.GetPagePos()] {
			p->ChangeSelection(size_t(pos));
		});
	};

	CreateButtons(wxAPPLY | wxOK | wxCANCEL);

	LayoutDialog();

	return true;
}

void SettingsDialog::SetServerInstanceId(const std::vector<uint8_t> id)
{
	server_instance_id_ = id;
}

const std::vector<uint8_t> &SettingsDialog::GetServerInstanceId() const
{
	return server_instance_id_;
}

void SettingsDialog::SetHostaddressAnyIsEquivalent(bool any_is_equivalent)
{
	any_is_equivalent_ = any_is_equivalent;
}

void SettingsDialog::SetGroupsAndUsers(const fz::authentication::file_based_authenticator::groups &groups, const fz::authentication::file_based_authenticator::users &users, bool can_impersonate, const fz::native_string server_username)
{
	groups_ = groups;
	users_ = users;
	can_impersonate_ = can_impersonate;
	server_username_ = server_username;

	groups_editor_->SetGroups(groups_);
	users_editor_->SetGroups(groups_);
	users_editor_->SetUsers(users_, server_name_);

	groups_editor_->SetServerPathFormat(server_path_format_);
	users_editor_->SetServerPathFormat(server_path_format_);

	authentication_page_->TransferDataToWindow();
}

std::pair<const fz::authentication::file_based_authenticator::groups &, const fz::authentication::file_based_authenticator::users &>
SettingsDialog::GetGroupsAndUsers() const
{
	return {groups_, users_};
}

void SettingsDialog::SetProtocolsOptions(const server_settings::protocols_options &protocols_options)
{
	protocols_options_ = protocols_options;

	autoban_max_login_failures_ctrl_->SetRef(protocols_options_.autobanner.max_login_failures());
	autoban_login_failures_time_window_ctrl_->SetRef(protocols_options_.autobanner.login_failures_time_window(), fz::duration::from_milliseconds(0));
	autoban_ban_duration_ctrl_->SetRef(protocols_options_.autobanner.ban_duration(), fz::duration::from_milliseconds(0));

	performance_number_of_session_threads_ctrl_->SetRef(protocols_options_.performance.number_of_session_threads, 0, 256);
	performance_receiving_buffer_size_ctrl_->SetRef(protocols_options_.performance.receive_buffer_size, -1)->set_mapping({{-1, _S("Use default")}});
	performance_sending_buffer_size_ctrl_->SetRef(protocols_options_.performance.send_buffer_size, -1)->set_mapping({{-1, _S("Use default")}});

	login_timeout_ctrl_->SetRef(protocols_options_.timeouts.login_timeout, fz::duration::from_milliseconds(0));
	activity_timeout_ctrl_->SetRef(protocols_options_.timeouts.activity_timeout, fz::duration::from_milliseconds(0));
}

const server_settings::protocols_options &SettingsDialog::GetProtocolsOptions() const
{
	return protocols_options_;
}

void SettingsDialog::SetFtpOptions(const fz::ftp::server::options &ftp_options, const fz::securable_socket::cert_info::extra &tls_extra_info)
{
	ftp_options_ = ftp_options;
	ftp_tls_extra_info_ = tls_extra_info;

	ftp_tls_min_ver_ctrl_->SetSelection(std::min(1, std::max(0, int(ftp_options_.sessions().tls.min_tls_ver) - int(fz::tls_ver::v1_2))));
	ftp_cert_info_ctrl_->SetValue(&ftp_options_.sessions().tls.cert, &ftp_tls_extra_info_, server_path_format_);

	static const auto ftp_list_info = [](const wxString &proto_name, fz::ftp::session::tls_mode m) {
		return AddressInfoListEditor::ListInfo::Info<fz::ftp::address_info>{
			proto_name,
			[m](const auto &ai) {
				return fz::ftp::address_info{ai, m};
			},
			[m](const auto &ai) {
				return ai.tls_mode == m;
			}
		};
	};

	server_listeners_editor_->SetLists({
		{ ftp_options_.listeners_info(), {
			 ftp_list_info(_S("Require explicit FTP over TLS"), fz::ftp::session::require_tls),
			 ftp_list_info(_S("Explicit FTP over TLS and insecure plain FTP"), fz::ftp::session::allow_tls),
			 ftp_list_info(_S("Implicit FTP over TLS (deprecated)"), fz::ftp::session::implicit_tls),
		}},
#ifdef ENABLE_FZ_WEBUI
		{ webui_options_.listeners_info, _S("WebUI") }
#endif
	});

	auto &range = ftp_options_.sessions().pasv.port_range;

	use_custom_port_range_ctrl_->SetValue(range.has_value());
	if (range.has_value()) {
		min_port_range_ctrl_->Enable();
		min_port_range_ctrl_->SetRef(range->min, 1, 65535);

		max_port_range_ctrl_->Enable();
		max_port_range_ctrl_->SetRef(range->max, 1, 65535);
	}
	else {
		min_port_range_ctrl_->Disable();
		min_port_range_ctrl_->SetRef(nullptr);

		max_port_range_ctrl_->Disable();
		max_port_range_ctrl_->SetRef(nullptr);
	}

	host_override_ctrl_->SetValue(fz::to_wxString(ftp_options_.sessions().pasv.host_override));
	disallow_host_override_for_local_peers_ctrl_->SetValidator(wxGenericValidator(&ftp_options_.sessions().pasv.do_not_override_host_if_peer_is_local));

	welcome_message_ctrl_->SetValidator(TextValidatorEx(wxFILTER_NONE, &ftp_options_.welcome_message(), [this](const wxString &str, bool is_final_validation) {
		wxString msg;

		if (is_final_validation) {
			auto welcome = fz::util::welcome_message(fz::to_utf8(str));
			if (welcome.empty() && !str.empty())
				return _S("Text contains invalid characters");

			auto res = welcome.validate();

			switch (res) {
				case decltype(res)::ok:
					break;

				case decltype(res)::line_too_long:
					msg = _F("Line cannot take up more than %zu bytes", decltype(res)::line_limit);
					break;

				case decltype(res)::total_size_too_big:
					msg = _F("Total size of message cannot exceeed %zu bytes", decltype(res)::total_limit);
					break;
			}

			if (!res) {
				auto utf8_from = res.data().data() - welcome.data();
				auto wstring_from = fz::to_wstring_from_utf8(welcome.substr(0, std::size_t(utf8_from))).size();
				auto wstring_to = wstring_from + fz::to_wstring_from_utf8(res.data()).size();

				welcome_message_ctrl_->CallAfter([wstring_from, wstring_to, this] {
					welcome_message_ctrl_->SetSelection(long(wstring_from), long(wstring_to));
					welcome_message_ctrl_->ShowPosition(long(wstring_from));
				});
			}
		}

		return msg;
	}));
}

std::pair<const fz::ftp::server::options &, const fz::securable_socket::cert_info::extra &> SettingsDialog::GetFtpOptions() const
{
	return {ftp_options_, ftp_tls_extra_info_};
}

bool SettingsDialog::IsFtpCertCurrentlyObfuscatedAndNotModified()
{
	return ftp_cert_info_ctrl_->IsCurrentlyObfuscatedAndNotModified();
}

bool SettingsDialog::SetFtpObfuscatedCert(const fz::securable_socket::cert_info &cert_info)
{
	return ftp_cert_info_ctrl_->SetObfuscatedCert(cert_info);
}

#ifdef ENABLE_FZ_WEBUI
void SettingsDialog::SetWebUIOptions(const fz::webui::server::options &webui_options, const fz::securable_socket::cert_info::extra &tls_extra_info)
{
	webui_options_ = webui_options;
	ftp_tls_extra_info_ = tls_extra_info;

	webui_tls_min_ver_ctrl_->SetSelection(std::min(1, std::max(0, int(webui_options_.tls.min_tls_ver) - int(fz::tls_ver::v1_2))));
	webui_cert_info_ctrl_->SetValue(&webui_options_.tls.cert, &ftp_tls_extra_info_, server_path_format_);

	static const auto ftp_list_info = [](const wxString &proto_name, fz::ftp::session::tls_mode m) {
		return AddressInfoListEditor::ListInfo::Info<fz::ftp::address_info>{
			proto_name,
			[m](const auto &ai) {
				return fz::ftp::address_info{ai, m};
			},
			[m](const auto &ai) {
				return ai.tls_mode == m;
			}
		};
	};

	server_listeners_editor_->SetLists({
		{ ftp_options_.listeners_info(), {
			ftp_list_info(_S("Require explicit FTP over TLS"), fz::ftp::session::require_tls),
			ftp_list_info(_S("Explicit FTP over TLS and insecure plain FTP"), fz::ftp::session::allow_tls),
			ftp_list_info(_S("Implicit FTP over TLS (deprecated)"), fz::ftp::session::implicit_tls),
		}},
		{ webui_options_.listeners_info, _S("WebUI") }
	});

	webui_access_token_timeout_ctrl_->SetRef(webui_options_.access_token_timeout, fz::duration::from_minutes(1), fz::duration::from_minutes(30));
	webui_refresh_token_timeout_ctrl_->SetRef(webui_options_.refresh_token_timeout, fz::duration::from_days(1), fz::duration::from_days(90));
}

std::pair<const fz::webui::server::options &, const fz::securable_socket::cert_info::extra &> SettingsDialog::GetWebUIOptions() const
{
	return {webui_options_, webui_tls_extra_info_};
}

bool SettingsDialog::IsWebUICertCurrentlyObfuscatedAndNotModified()
{
	return webui_cert_info_ctrl_->IsCurrentlyObfuscatedAndNotModified();
}

bool SettingsDialog::SetWebUIObfuscatedCert(const fz::securable_socket::cert_info &cert_info)
{
	return webui_cert_info_ctrl_->SetObfuscatedCert(cert_info);
}
#endif

void SettingsDialog::SetAdminOptions(const server_settings::admin_options &admin_options, const fz::securable_socket::cert_info::extra &tls_extra_info)
{
	admin_options_ = admin_options;
	admin_tls_extra_info_ = tls_extra_info;

	admin_port_ctrl_->SetRef(admin_options_.local_port, 1, 65535);
	admin_password_ctrl_->SetPassword(&admin_options_.password);
	admin_listeners_editor_->SetLists({
		{ admin_options_.additional_address_info_list, _S("Administration") }
	});

	admin_cert_info_ctrl_->SetValue(&admin_options_.tls.cert, &admin_tls_extra_info_, server_path_format_);

	admin_listeners_editor_->Enable(admin_options_.password.has_password());
}

std::pair<const server_settings::admin_options &, const fz::securable_socket::cert_info::extra &> SettingsDialog::GetAdminOptions() const
{
	return {admin_options_, admin_tls_extra_info_};
}

bool SettingsDialog::IsAdminCertCurrentlyObfuscatedAndNotModified()
{
	return admin_cert_info_ctrl_->IsCurrentlyObfuscatedAndNotModified();
}

bool SettingsDialog::SetAdminObfuscatedCert(const fz::securable_socket::cert_info &cert_info)
{
	return admin_cert_info_ctrl_->SetObfuscatedCert(cert_info);
}

void SettingsDialog::SetFilters(const fz::tcp::binary_address_list &disallowed_ips, const fz::tcp::binary_address_list &allowed_ips)
{
	disallowed_ips_ = disallowed_ips;
	allowed_ips_ = allowed_ips;

	filters_ctrl_->SetIps(&disallowed_ips_, &allowed_ips_);
}

std::pair<const fz::tcp::binary_address_list &, const fz::tcp::binary_address_list &> SettingsDialog::GetFilters() const
{
	return {disallowed_ips_, allowed_ips_};
}

void SettingsDialog::SetLoggingOptions(const fz::logger::file::options &opts)
{
	logger_opts_ = opts;

	logging_choice_ctrl_->Enable();
	logging_choice_ctrl_->SetSelection(opts.name().empty());
	log_rotation_choice_ctrl_->SetSelection(opts.max_amount_of_rotated_files() > 0);
	log_rotation_type_ctrl_->SetSelection(opts.rotation_type());

	SetLogTypes(logger_opts_.enabled_types());
	log_level_old_selection_ = log_level_ctrl_->GetSelection();

	log_path_ctrl_->SetValidator(TextValidatorEx(wxFILTER_NONE, &logger_opts_.name(), absolute_path_validation(server_path_format_)));
	log_rotations_amount_ctrl_->SetRef(logger_opts_.max_amount_of_rotated_files(), 1, 10000);
	log_file_size_ctrl_->SetRef(logger_opts_.max_file_size(), 1*1024*1024)->set_mapping({
		{ logger_opts_.max_possible_size, _S("Maximum possible") }
	});
	log_include_headers_ctrl_->SetValidator(wxGenericValidator(&logger_opts_.include_headers()));
	log_date_in_name_->SetValidator(wxGenericValidator(&logger_opts_.date_in_name()));
}

const fz::logger::file::options &SettingsDialog::GetLoggingOptions() const
{
	return logger_opts_;
}

void SettingsDialog::SetAcmeAccountId(const std::string &account_id, const fz::acme::extra_account_info &extra)
{
	acme_opts_.account_id = account_id;
	acme_extra_account_info_ = extra;

	if (!extra)
		acme_opts_.account_id.clear();

	acme_account_id_button_->Enable();
	acme_account_id_button_->SetLabel(_S("&Generate new account"));

	if (acme_opts_.account_id.empty()) {
		auto font = acme_account_id_ctrl_->GetFont();
		font.SetStyle(wxFONTSTYLE_ITALIC);

		acme_account_id_ctrl_->SetFont(font);
		acme_account_id_ctrl_->SetValue(_S("<Account not created yet>"));
	}
	else {
		auto font = acme_account_id_ctrl_->GetFont();
		font.SetStyle(wxFONTSTYLE_NORMAL);

		acme_account_id_ctrl_->SetFont(font);
		acme_account_id_ctrl_->SetValue(fz::to_wxString(account_id));
	}

	acme_account_directory_ctrl_->SetValue(fz::to_wxString(extra.directory));
	acme_account_contacts_ctrl_->SetValue(fz::join<wxString>(extra.contacts));

	acme_account_id_ctrl_->Enable();
	acme_account_directory_ctrl_->Enable();
	acme_account_contacts_ctrl_->Enable();
	acme_account_id_button_->Enable();
}

void SettingsDialog::SetAcmeOptions(const server_settings::acme_options &acme_options, const fz::acme::extra_account_info &extra)
{
	acme_opts_ = acme_options;
	acme_extra_account_info_ = extra;

	if (!extra)
		acme_opts_.account_id.clear();

	SetAcmeAccountId(acme_opts_.account_id, extra);

	ftp_cert_info_ctrl_->SetAcmeOptions(acme_opts_);
	//admin_cert_info_ctrl_->SetAcmeOptions(acme_opts_);

	#ifdef ENABLE_FZ_WEBUI
		webui_cert_info_ctrl_->SetAcmeOptions(acme_opts_);
	#endif

	if (auto h = std::get_if<fz::acme::serve_challenges::internally>(&acme_opts_.how_to_serve_challenges)) {
		acme_how_ctrl_->ChangeSelection(1);
		acme_listeners_ = h->addresses_info;
	}
	else
	if (auto h = std::get_if<fz::acme::serve_challenges::externally>(&acme_opts_.how_to_serve_challenges)) {
		acme_how_ctrl_->ChangeSelection(2);
		acme_well_known_path_ = h->well_known_path;
		acme_create_path_if_not_exist_ctrl_->SetValue(h->create_if_not_existing);
		acme_listeners_.clear();
	}
	else
		acme_how_ctrl_->ChangeSelection(0);

	acme_enabled_->SetValue(acme_opts_.how_to_serve_challenges && extra);
	auto evt = wxCommandEvent(wxEVT_CHECKBOX);
	evt.SetInt(acme_opts_.how_to_serve_challenges && extra);
	acme_enabled_->GetEventHandler()->ProcessEvent(evt);

	acme_well_knon_path_ctrl_->SetValidator(TextValidatorEx(wxFILTER_NONE, &acme_well_known_path_, absolute_path_validation(server_path_format_)));

	acme_listeners_ctrl_->SetLists({
		{acme_listeners_, _S("HTTP")}
	});
}

const server_settings::acme_options &SettingsDialog::GetAcmeOptions() const
{
	return acme_opts_;
}

void SettingsDialog::SetPkcs11Options(const server_settings::pkcs11_options &pkcs11_options)
{
	pkcs11_options_ = pkcs11_options;
}

const server_settings::pkcs11_options &SettingsDialog::GetPkcs11Options() const
{
	return pkcs11_options_;
}

#ifdef ENABLE_FZ_UPDATE_CHECKER
void SettingsDialog::SetUpdatesOptions(const fz::update::checker::options &opts)
{
	updates_options_ = opts;
}

const fz::update::checker::options &SettingsDialog::GetUpdatesOptions() const
{
	return updates_options_;
}

void SettingsDialog::SetUpdateInfo(const fz::update::info &info, fz::datetime last_check, fz::datetime next_check)
{
	update_info_ = info;
	update_last_check_ = last_check;
	update_next_check_ = next_check;

	if (update_check_button_ && update_check_func_)
		update_check_button_->Enable();
}

void SettingsDialog::SetUpdateCheckFunc(UpdateCheckFunc func)
{
	update_check_func_ = std::move(func);
}
#endif

void SettingsDialog::SetApplyFunction(std::function<bool ()> func)
{
	apply_function_ = std::move(func);
}

void SettingsDialog::SetGenerateSelfsignedCertificateFunction(CertInfoEditor::GenerateSelfsignedFunc func)
{
	ftp_cert_info_ctrl_->SetGenerateSelfsignedCertificateFunction(func);
	admin_cert_info_ctrl_->SetGenerateSelfsignedCertificateFunction(std::move(func));

	#ifdef ENABLE_FZ_WEBUI
		webui_cert_info_ctrl_->SetGenerateSelfsignedCertificateFunction(func);
	#endif

}

void SettingsDialog::SetGenerateAcmeCertificateFunction(CertInfoEditor::GenerateAcmeFunc func)
{
	ftp_cert_info_ctrl_->SetGenerateAcmeCertificateFunction(func);
	admin_cert_info_ctrl_->SetGenerateAcmeCertificateFunction(func);

	#ifdef ENABLE_FZ_WEBUI
		webui_cert_info_ctrl_->SetGenerateAcmeCertificateFunction(func);
	#endif
}

void SettingsDialog::SetTestCertificateFunction(CertInfoEditor::TestCertificateFunc func)
{
	ftp_cert_info_ctrl_->SetTestCertificateFunction(func);
	admin_cert_info_ctrl_->SetTestCertificateFunction(func);

	#ifdef ENABLE_FZ_WEBUI
		webui_cert_info_ctrl_->SetTestCertificateFunction(func);
	#endif
}

void SettingsDialog::SetRetrieveDeobfuscatedBlobFunction(CertInfoEditor::RetrieveDeobfuscatedBlobFunc func)
{
	ftp_cert_info_ctrl_->SetRetrieveDeobfuscatedBlobFunction(func);
	admin_cert_info_ctrl_->SetRetrieveDeobfuscatedBlobFunction(func);

	#ifdef ENABLE_FZ_WEBUI
		webui_cert_info_ctrl_->SetRetrieveDeobfuscatedBlobFunction(func);
	#endif
}

#ifdef ENABLE_FZ_WEBUI
void SettingsDialog::SetDestroyWebUITokensFunction(std::function<wxString ()> func)
{
	webui_destroy_tokens_func_ = std::move(func);
}
#endif

void SettingsDialog::SetGenerateAcmeAccountFunction(GenerateAcmeAccountFunc func)
{
	acme_generate_account_func_ = std::move(func);
}

void SettingsDialog::SetGetPublicIpFunc(GetPublicIpFunc func)
{
	get_public_ip_func_ = std::move(func);
}

bool SettingsDialog::TransferDataFromWindow()
{
	server_listeners_editor_->SetAddressAndPortValidator([this](const wxString &address, unsigned int port) {
		if (admin_password_ctrl_->HasPassword()) {
			if (auto m = admin_listeners_editor_->GetMatchingAddress(address, port, any_is_equivalent_); !m.empty())
				return address_and_port_in_other_editor(address, port, m, wxGetContainingPageTitle(admin_listeners_editor_));
		}

		fz::hostaddress h1(address.ToStdWstring(), fz::hostaddress::format::ipvx, port);

		unsigned int admin_port = 0;
		admin_port_ctrl_->Get(admin_port);

		for (std::string_view a: { "127.0.0.1", "::1" }) {
			fz::hostaddress h2(a, fz::hostaddress::format::ipvx, admin_port);
			if (h1.equivalent_to(h2, any_is_equivalent_))
				return address_and_port_in_other_editor(address, port, wxEmptyString, wxGetContainingPageTitle(admin_port_ctrl_));
		}

		return wxString();
	});

	auto pasv_transferer = wxTransferDataFromWindow(pasv_page_, [this] {
		if (auto &range = ftp_options_.sessions().pasv.port_range) {
			for (auto *e: {server_listeners_editor_, admin_listeners_editor_}) {
				if (auto p = e->GetFirstPortInRange(range->min, range->max)) {
					wxMsg::Error(_F("Passive Mode port range conflicts with port %d in %s", p, wxGetContainingPageTitle(e)));
					return false;
				}
			}

			unsigned int admin_port = 0;
			admin_port_ctrl_->Get(admin_port);

			if (range->min <= admin_port && admin_port <= range->max) {
				wxMsg::Error(_F("Passive Mode port range conflicts with port %d in %s", admin_port, wxGetContainingPageTitle(admin_port_ctrl_)));
				return false;
			}

			if (range->min < 1024) {
				if (wxMsg::Confirm(_S("It's highly discouraged to use ports below 1024 for the Passive Mode port range.")).Ext(_S("Proceed anyway?")).Title(_S("Warning: privileged port requested")) == wxID_NO)
					return false;
			}
		}

		return true;
	});

	bool can_transfer = wxDialogEx::TransferDataFromWindow();

	server_listeners_editor_->SetAddressAndPortValidator(nullptr);
	pasv_transferer->Destroy();

	if (!can_transfer)
		return false;

	ftp_options_.sessions().tls.min_tls_ver = fz::tls_ver(ftp_tls_min_ver_ctrl_->GetSelection() + int(fz::tls_ver::v1_2));
	admin_options_.tls.min_tls_ver = fz::tls_ver::v1_3;

	#ifdef ENABLE_FZ_WEBUI
		webui_options_.tls.min_tls_ver = fz::tls_ver(webui_tls_min_ver_ctrl_->GetSelection() + int(fz::tls_ver::v1_2));
	#endif

	if (log_level_old_selection_ != log_level_ctrl_->GetSelection()) {
		log_level_old_selection_ = log_level_ctrl_->GetSelection();
		logger_opts_.enabled_types(GetLogTypes());
	}

	check_expired_certs();

	if (apply_function_)
		return apply_function_();

	return true;
}

wxBookCtrlBase *SettingsDialog::CreateBookCtrl()
{
	// All this reparenting stuff is needed on Windows to make the mnemonics work as intended,
	// since the handling of mnemonics is native there, and apparently works by focusing on whichever is the widget
	// that was created just after to the one with the mnemonics.
	//
	// It's hackish, and without peeking into wx source code I wouldn't have known how to do this, but it works.

	auto title = Title(this, _S("S&elect a page:"));

	struct Treebook: wxTreebook
	{
		Treebook(wxWindow *parent, wxWindow *title)
			: wxTreebook(parent, wxID_ANY)
		{
			// We assume wxTreeCtrl doesn't create its own sizer
			wxASSERT(GetSizer() == nullptr);

			title->Reparent(this);

			m_controlSizer = wxHBox(this, 0) = {
				wxSizerFlags(0).Expand() >>= wxVBox(this, 0) = {
					wxSizerFlags(0) >>= title,
					wxSizerFlags(1) >>= m_bookctrl
				}
			};

			title->MoveBeforeInTabOrder(m_bookctrl);
			SetSizer(m_controlSizer);
		}
	};

	auto book = wxCreate<Treebook>(this, title);

	return book;
}

void SettingsDialog::AddBookCtrl(wxSizer *sizer)
{
	sizer->Add(GetBookCtrl(), wxSizerFlags(1).Expand().Border(wxALL, wxDlg2Px(this, wxDefaultPadding)));
}

void SettingsDialog::SetLogTypes(fz::logmsg::type types)
{
	int level = 0;

	if (fz::logmsg::debug_debug & types)
		level = 5;
	else
	if (fz::logmsg::debug_verbose & types)
		level = 4;
	else
	if (fz::logmsg::debug_info & types)
		level = 3;
	else
	if (fz::logmsg::debug_warning & types)
		level = 2;
	else
	if (types != fz::logmsg::type{})
		level = 1;

	log_level_ctrl_->SetSelection(level);
}

fz::logmsg::type SettingsDialog::GetLogTypes()
{
	std::make_unsigned_t<fz::logmsg::type> types{};

	int level = log_level_ctrl_->GetSelection();

	if (5 <= level)
		types |= fz::logmsg::debug_debug;

	if (4 <= level)
		types |= fz::logmsg::debug_verbose;

	if (3 <= level)
		types |= fz::logmsg::debug_info;

	if (2 <= level)
		types |= fz::logmsg::debug_warning;

	if (1 <= level)
		types |= fz::logmsg::default_types;

	return fz::logmsg::type(types);
}


void SettingsDialog::OnGroupsListChanging(GroupsList::Event &ev)
{
	// If adding a new group, nothing must be done here.
	if (ev.GetOldName().empty())
		return;

	std::string new_name = ev.GetNewName();
	bool questions_asked = false;
	std::string group_to_move_user_to;
	bool users_have_been_erased = true;

	enum state_e {
		delete_group = 1,
		delete_user_if_it_has_no_groups_left,
		rename_group,
		move_to_group,
		abort
	} state{};

	auto ask_user_what_to_do_on_deletion = [&] () -> state_e {
		std::vector<std::string_view> alternative_groups;
		alternative_groups.reserve(groups_.size()-1);
		for (const auto &g: groups_) {
			if (g.first != ev.GetOldName())
				alternative_groups.push_back(g.first);
		}

		wxArrayString choices;
		choices.reserve(groups_.size()+1);
		choices.push_back(_F("Delete the group \"%s\"", ev.GetOldName()));
		choices.push_back(_F("Delete all users that only belong to the group \"%s\"", ev.GetOldName()));

		for (const auto &g: alternative_groups) {
			choices.push_back(_F("Move users to group \"%s\"", fz::to_wxString(g)));
		}

		// Creating a new window makes the old widget lose focus. We don't want that.
		wxAutoRefocuser refocus;

		int idx = wxGetSingleChoiceIndex(
			_F("Some users still belong to the group \"%s\".\n\n"
			   "Hit CANCEL to block the operation,\n"
				"or choose an option from the list.", ev.GetOldName()),
			_F("Are you sure you want to delete group \"%s\"?", ev.GetOldName()),
			choices,
			0
		);

		if (idx < 0)
			return abort;

		if (idx == 0)
			return delete_group;

		if (idx == 1)
			return delete_user_if_it_has_no_groups_left;

		new_name = alternative_groups[std::size_t(idx-2)];
		return move_to_group;
	};

	if (!new_name.empty())
		state = rename_group;
	else
		state = delete_group;

	for (auto uit = users_.begin(), uend = users_.end(); uit != uend;) {
		bool erase_user = false;

		auto &groups = uit->second.groups;

		auto group_name_it = std::find(groups.begin(), groups.end(), ev.GetOldName());
		if (group_name_it != groups.end()) {
			check_what_to_do:

			switch (state) {
				// Bare renaming is allowed without much fuss
				case rename_group:
					*group_name_it = new_name;
					break;

				// Deleting, on the other hand, must preserve data consistency and we need to ask the user a few questions about it.
				case delete_group:
					if (!questions_asked) {
						state = ask_user_what_to_do_on_deletion();
						questions_asked = true;
					}

					if (state != delete_group)
						goto check_what_to_do;

					groups.erase(group_name_it);
					break;

				case abort:
					ev.Veto();
					return;

				case delete_user_if_it_has_no_groups_left:
					if (groups.size() == 1)
						erase_user = true;
					else
						groups.erase(group_name_it);

					break;

				// Moving is kind of like renaming, but we also need to make sure to not create duplicates: perhaps the user is already in that group!
				case move_to_group:
					if (std::find(groups.begin(), groups.end(), new_name) == groups.end())
						*group_name_it = new_name;
					break;
			}
		}

		if (erase_user) {
			uit = users_.erase(uit);
			users_have_been_erased = true;
		}
		else {
			++uit;
		}
	}

	if (users_have_been_erased)
		users_editor_->SetUsers(users_, server_name_);
}

void SettingsDialog::check_expired_certs()
{
	std::vector<wxString> protocols_with_expired_certs;

	if (ftp_tls_extra_info_.expired())
		protocols_with_expired_certs.push_back(wxT("FTP"));

	if (admin_tls_extra_info_.expired())
		protocols_with_expired_certs.push_back(_S("Administration"));

	if (!protocols_with_expired_certs.empty())
		wxMsg::Error(_S("The TLS certificates for the following protocols have expired: %s."), fz::join(protocols_with_expired_certs, wxT(", ")));
}

