#include <libfilezilla/glue/wx.hpp>

#include <wx/statline.h>
#include <wx/valgen.h>
#include <wx/gauge.h>
#include <wx/evtloop.h>
#include <wx/dirdlg.h>

#include "../serveradministrator.hpp"
#include "../helpers.hpp"
#include "../../src/filezilla/util/scope_guard.hpp"
#include "../settings.hpp"
#include "../textvalidatorex.hpp"
#include "../downloader.hpp"
#include "../settingsdialog.hpp"
#include "../../filezilla/build_info.hpp"

#ifdef ENABLE_FZ_UPDATE_CHECKER
bool ServerAdministrator::CheckForUpdates()
{
	if (!IsConnected())
		return false;

	logger_.log_u(fz::logmsg::status, _S("Checking for updates..."));

	waiting_for_updates_info_ = true;
	client_.send<administration::solicit_update_info>();
	return true;
}

void ServerAdministrator::Dispatcher::operator()(administration::update_info &&v)
{
	invoke_on_admin([this, v = std::move(v)]() {
		fz::util::scope_guard on_return([&] {
			server_admin_.waiting_for_updates_info_ = false;
		});

		server_admin_.logger_.log_u(server_admin_.waiting_for_updates_info_ ? fz::logmsg::status : fz::logmsg::debug_warning, _S("Received update information from server."));

		auto &&[expected_info, last_check, next_check] = std::move(v).tuple();

		auto next_check_msg = next_check ? _F("The next automatic check will be performed at %s", fz::to_wxString(next_check)) : wxT("");

		auto msg = wxMsg::Builder().Title(_S("Server update")).JustLog(!server_admin_.waiting_for_updates_info_).Ext(next_check_msg);

		bool show_dialog = false;

		server_admin_.server_update_last_check_ = last_check;
		server_admin_.server_update_next_check_ = next_check;

		if (!expected_info)
			msg.Error(_S("Error retrieving update information: %s"), expected_info.error());
		else {
			auto &info = *expected_info;

			bool same_old_data = info == server_admin_.server_update_info_;

			server_admin_.server_update_info_ = info;

			if (same_old_data && !server_admin_.waiting_for_updates_info_);
			else
			if (!info.is_newer_than(server_admin_.server_version_))
				msg.Success(_S("No new version is available at the moment."));
			else
			if (!info);
			else
			if (!info->version)
				msg.Success(_S("The current version of %s has reached its end of life. You will no longer get any automatic update."));
			else
				show_dialog = true;
		}

		if (server_admin_.settings_dialog_)
			server_admin_.settings_dialog_->SetUpdateInfo(server_admin_.server_update_info_, server_admin_.server_update_last_check_, server_admin_.server_update_next_check_);

		if (!show_dialog)
			return;

		wxPushDialog(wxTheApp->GetTopWindow(), wxID_ANY, _S("Server update"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		| [this](wxDialog *p) {
			enum {
				cannot_download_page,
				select_directory_page,
				download_page,
				downloaded_page
			};

			auto &info = server_admin_.server_update_info_;

			wxSimplebook *book;
			wxHyperlinkCtrl *install_link;
			wxString package_location;
			bool can_download = true;

			wxVBox(p) = {
				wxLabel(p, _F("A new version of %s is now available: %s.", fz::build_info::package_name, info->version)),

				wxLabel(p, info->changelog.empty() ? _S("No changelog is available.") : _S("Chan&gelog:")),
				wxSizerFlags(1).Expand() >>= info->changelog.empty() ? nullptr
				:  new wxTextCtrl(p, wxID_ANY, fz::to_wxString(info->changelog), wxDefaultPosition, wxMonospaceTextExtent(100, 20, p, {wxSYS_VSCROLL_X}), wxTE_MULTILINE | wxTE_READONLY) | [&](auto p) {
					auto font = p->GetFont();
					font.SetFamily(wxFONTFAMILY_TELETYPE);
					p->SetFont(font);
					p->SetInsertionPoint(0);
				},

				wxCreate<wxNavigationEnabled<wxSimplebook>>(p) |[&](auto p) {
					book = p;

					wxPage(wxValidateOnlyIfCurrent)(p, wxT("**CANNOT DOWNLOAD**")) | [](auto p) {
						wxHBox(p, 0) = {
							new wxStaticText(p, wxID_ANY, _S("Cannot automatically do&wnload this release package.")),
							new wxHyperlinkCtrl(p, wxID_ANY,
								_F("Download it from the official %s web page.", fz::build_info::package_name),
								wxT("https://filezilla-project.org/download.php?type=server"), wxDefaultPosition, wxDefaultSize, wxHL_ALIGN_LEFT)
						};
					};

					wxPage(wxValidateOnlyIfCurrent)(p, wxT("**SELECT DOWNLOAD**")) | [&, book = p](auto p) {
						wxTextCtrl *dir;
						wxCheckBox *check;
						wxButton *download;
						wxButton *browse;

						wxHBox(p, 0) = {
							new wxStaticText(p, wxID_ANY, _S("Download &location:")),
							wxSizerFlags(3) >>= dir = new wxTextCtrl(p, wxID_ANY),
							browse = new wxButton(p, wxID_ANY, _S("Bro&wse...")),
							download = new wxButton(p, wxID_ANY, _S("&Download")),
							wxEmptySpace,
							check = new wxCheckBox(p, wxID_ANY, _S("Start download automatically next &time.")),
						};

						browse->Bind(wxEVT_BUTTON, [browse, dir, p](wxCommandEvent &) {
							browse->Disable();

							wxPushDialog<wxDirDialog>(p, _S("Choose a place where to download the update."), dir->GetValue()) | [&](wxDirDialog &d) {
								auto ret = d.ShowModal();

								browse->Enable();

								if (ret == wxID_CANCEL)
									return;

								dir->SetValue(d.GetPath());
							};
						});

						dir->Bind(wxEVT_TEXT, [check, download](wxCommandEvent &ev) {
							fz::util::fs::native_path path = fz::to_native(ev.GetString());

							bool must_set = path.is_absolute() && path.type() == fz::local_filesys::dir;

							check->Enable(must_set);
							download->Enable(must_set);

							if (must_set)
								Settings()->updates_dir() = std::move(path);
						});

						check->SetValidator(wxGenericValidator(&Settings()->automatically_download_updates()));

						download->Bind(wxEVT_BUTTON, [book](wxCommandEvent &) {
							book->SetSelection(download_page);
						});

						dir->SetValue(fz::to_wxString(Settings()->updates_dir().str_view()));
					};

					wxPage(wxValidateOnlyIfCurrent)(p, wxT("**DO DOWNLOAD**")) | [&](auto p) mutable {
						Downloader *downloader;

						auto http_opts = fz::http::client::options()
							.follow_redirects(true)
							.default_timeout(fz::duration::from_seconds(30))
							.cert_verifier(fz::http::client::do_not_verify);

						wxHBox(p, 0) = downloader = new Downloader(p, server_admin_.pool_, server_admin_.loop_, &server_admin_.logger_, std::move(http_opts));

						wxTransferDataToWindow(p, [&info, downloader, &can_download] {
							if (!(info->file && Settings()->updates_dir().is_absolute() && Settings()->updates_dir().type() == fz::local_filesys::dir))
								return false;

							if (can_download) {
								can_download = false;

								downloader->SetDownloadDir(Settings()->updates_dir());
								downloader->SetUrl(info->file->url, info->file->size, info->file->hash);
								downloader->Start();
							}

							return true;
						});

						downloader->Bind(Downloader::Event::Stopped, [&](Downloader::Event &ev) {
							if (!ev.GetReason().empty())
								wxMsg::Error(_S("Error while downloading the update package.")).Ext(ev.GetReason());

							can_download = true;
							book->SetSelection(select_directory_page);
						});

						downloader->Bind(Downloader::Event::Succeeded, [&](Downloader::Event &ev) {
							can_download = true;

							auto path = fz::util::fs::wpath(ev.GetReason().ToStdWstring());

							if (install_link) {
								std::wstring link;

								#ifdef FZ_WINDOWS
									if (fz::ends_with(path.str_view(), std::wstring_view(L".exe")))
										link = path;
									else
								#endif

								link = fz::sprintf(L"file://%s", fz::percent_encode(path, true));

								install_link->SetURL(fz::to_wxString(link));
							}

							package_location = fz::to_wxString(fz::sprintf(L"file://%s", fz::percent_encode(path.parent(), true)));

							book->SetSelection(downloaded_page);
						});
					};

					wxPage(wxValidateOnlyIfCurrent)(p, wxT("**DONWLOAD AVAILABLE**")) | [&](auto p) {
						wxButton *change_parameters;
						wxButton *open_location;

						wxHBox(p, 0) = {
							new wxStaticText(p, wxID_ANY, _S("The &update package has been successfully downloaded.")),
							install_link = server_admin_.server_host_ != fz::build_info::host ? nullptr : new wxHyperlinkCtrl(p, wxID_ANY, _S("You can now install it."),	wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_ALIGN_LEFT),
							wxEmptySpace,
							open_location = new wxButton(p, wxID_ANY, _S("Open package &location")),
							change_parameters = new wxButton(p, wxID_ANY, _S("C&hange parameters"))
						};

						open_location->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
							wxLaunchDefaultBrowser(package_location);
						});

						change_parameters->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
							book->SetSelection(select_directory_page);
						});

						#ifdef FZ_WINDOWS
							if (install_link) {
								install_link->Bind(wxEVT_HYPERLINK, [&](wxHyperlinkEvent &ev) {
									if (ev.GetURL().StartsWith(wxT("file://")))
										wxLaunchDefaultBrowser(ev.GetURL());
									else
									if (!wxLaunchDefaultApplication(ev.GetURL())) {
										if (auto e = GetLastError(); e != ERROR_CANCELLED)
											wxMsg::Error(_F("Could not start installer '%s': %d", ev.GetURL(), e));
									}
								});
							}
						#endif
					};
				},

				new wxStaticLine(p),
				p->CreateButtonSizer(wxCLOSE)
			};

			if (!info->file)
				book->SetSelection(cannot_download_page);
			else  {
				book->SetSelection(select_directory_page);

				if (Settings()->automatically_download_updates())
					book->SetSelection(download_page);
			}

			wxValidate(p, [&] {
				int proceed = wxID_YES;

				if (book->GetSelection() == download_page)
					proceed = wxMsg::Confirm(_S("Closing the window will abort the download")).Ext(_S("Do you want to proceed anyway?"));

				return proceed == wxID_YES;
			});

			p->ShowModal();

			Settings().Save();
		};
	});
}


void ServerAdministrator::Dispatcher::operator()(administration::get_updates_options::response && v)
{
	invoke_on_admin([this, v = std::move(v)] {
		if (!v) {
			wxMsg::Error(_S("Error retrieving updates options"));
			return;
		}

		if (!was_expecting_response())
			return;

		std::tie(server_admin_.updates_options_) = std::move(*v).tuple();

		server_admin_.maybe_act_on_settings_received();
	});
}

void ServerAdministrator::Dispatcher::operator()(administration::retrieve_update_raw_data &&v)
{
	auto &&[query_string] = std::move(v).tuple();

	http_update_retriever_.retrieve_raw_data(query_string, fz::async_receive(async_handler_.get()) >> [&](auto &expected_raw_data) {
		server_admin_.client_.send<administration::retrieve_update_raw_data::response>(std::move(expected_raw_data));
	});
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::get_updates_options::response);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::update_info);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, ServerAdministrator::Dispatcher, administration::retrieve_update_raw_data);
#endif
