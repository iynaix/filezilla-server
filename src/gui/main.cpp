#include <libfilezilla/glue/wx.hpp>

#include "../filezilla/logger/file.hpp"
#include "../filezilla/known_paths.hpp"
#include "../filezilla/build_info.hpp"
#include "../filezilla/util/tools.hpp"
#include "../filezilla/tls_exit.hpp"

#include "locale.hpp"
#include "glue.hpp"

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/aboutdlg.h>
#include <wx/simplebook.h>
#include <wx/button.h>
#include <wx/display.h>
#include <wx/taskbar.h>

#include "serveradministrator.hpp"
#include "connectiondialog.hpp"
#include "helpers.hpp"
#include "trayicon.hpp"
#include "settings.hpp"

class Application : public wxApp
{
public:
	virtual bool OnInit() override;

private:
	fz::logger::file logger_{};
	wxLocale locale_;
};

class MainFrame : public wxFrame
{
public:
	MainFrame(fz::logger_interface &logger);

	void TryToAutoConnect();

private:
	void OnConnect(wxCommandEvent& event);
	void OnDisconnect(wxCommandEvent& event);
	void OnConfigure(wxCommandEvent& event);
	void OnConfigureNetwork(wxCommandEvent& event);
	void OnTestNetwork(wxCommandEvent& event);
	void OnExportConfig(wxCommandEvent& event);
	void OnImportConfig(wxCommandEvent& event);

#ifdef ENABLE_FZ_UPDATE_CHECKER
	void OnCheckForUpdates(wxCommandEvent &);
#endif

	void OnExit(wxCommandEvent& event);

	void OnClose(wxCloseEvent &event);

	void OnStartMinimized(wxCommandEvent& event);
	void OnEnableTrayIcon(wxCommandEvent& event);
	void OnMinimizeToTray(wxCommandEvent& event);

	void OnAbout(wxCommandEvent& event);
	void OnResetUIConfig(wxCommandEvent &event);

	/****/
	void OnAdminConnection(ServerAdministrator::Event &);
	void OnAdminDisconnection(ServerAdministrator::Event &);
	void OnAdminReconnection(ServerAdministrator::Event &);
	void OnServerInfoUpdated(ServerAdministrator::Event &);

	enum page: size_t {
		connected    = 0,
		disconnected = 1
	};

	static const int reconnect_timeout = 2;

	fz::logger_interface &logger_;

	wxSimplebook *book_;
	ServerAdministrator *administrator_;
	wxButton *connect_button_{};

	wxMenuItem *configure_menu_{};
	wxMenuItem *disconnect_menu_{};
	wxMenuItem *connect_menu_{};
	wxMenuItem *network_menu_{};
	wxMenuItem *test_menu_{};
	wxMenuItem *export_menu_{};
	wxMenuItem *import_menu_{};
	wxMenuItem *check_for_updates_menu_{};
	wxMenuItem *enable_tray_icon_menu_{};
	wxMenuItem *minimize_to_tray_menu_{};

	bool close_on_disconnect_{};

	std::unique_ptr<TrayIcon> tray_icon_{};
	void enable_tray_icon(bool enable);
	void enable_minimize_to_tray(bool enable);

	wxIcon GetIcon();
};


enum
{
	ID_Connect = 1,
	ID_Configure,
	ID_Disconnect,
	ID_ConfigureNetwork,
	ID_TestNetwork,
	ID_ExportConfig,
	ID_ImportConfig,

	ID_StartMinimized,
	ID_EnableTrayIcon,
	ID_MinimizeToTray,

	ID_Update,
	ID_ResetUIConfig
};

wxIMPLEMENT_WX_THEME_SUPPORT
wxIMPLEMENT_APP_NO_MAIN(Application);

int main(int argc, char **argv)
{
	//wxDISABLE_DEBUG_SUPPORT();
	fz::tls_exit(wxEntry(argc, argv));
}

bool Application::OnInit()
{
	wxSetWindowLogger(nullptr, &logger_);
	/*
	wxLog::SetLogLevel(wxLOG_Max);
	wxLog::SetComponentLevel(wxT("wx"), wxLOG_Max);
	wxLog::EnableLogging();
	wxLog::AddTraceMask(wxT("focus"));
*/
	if (!Settings().Load(argc, argv))
		return false;

	wxIdleEvent::SetMode(wxIDLE_PROCESS_SPECIFIED);
	wxUpdateUIEvent::SetMode(wxUPDATE_UI_PROCESS_SPECIFIED);

	{
		wxLogNull dont_log;

		int lang = wxLANGUAGE_DEFAULT;
		if (auto info = locale_.FindLanguageInfo(fz::to_wxString(Settings()->locale())))
			lang = info->Language;
		else
			wxMsg::Warning(_F("Couldn't find info about locale '%s'.", Settings()->locale()));

		if (!locale_.Init(lang))
			wxMsg::Warning(_F("Couldn't initialize locale to %d.", lang));
	}

	logger_.set_options(Settings()->logger());

	MainFrame *frame = new MainFrame(logger_);

	if (!Settings()->start_minimized())
		frame->Show(true);
	else
	if (Settings()->tray_icon_mode() != Settings()->minimize_to_tray) {
		frame->Show(true);
		frame->Iconize(true);
	}

	if (!fz::build_info::warning_message.empty())
		wxMsg::Warning(fz::join<wxString>(fz::strtokenizer(fz::build_info::warning_message, "\n\r", false), wxT("\n\n")));

	frame->TryToAutoConnect();

	return true;
}

static wxIcon get_logo(wxCoord coord)
{
#if defined(FZ_UNIX)
	static auto search_dirs =
		(fz::known_paths::data::home() +
		(fz::util::get_own_executable_directory().parent() / fzT("share")) +
		fz::known_paths::data::dirs()) / fzT("icons") / fzT("hicolor");

	auto size = fz::sprintf(fzT("%dx%d"), coord, coord);
	auto path = (search_dirs / size).resolve(fzT("apps/filezilla-server-gui.png"), fz::file::reading);

	if (!path.is_valid())
		return {};

	return wxIcon(fz::to_wxString(path.str()), wxBITMAP_TYPE_PNG);
#elif defined (FZ_MAC)
	return wxIcon(wxT("filezilla-server-gui"), wxBITMAP_TYPE_ICON_RESOURCE, coord, coord);
#elif defined (FZ_WINDOWS)
	return wxIcon(wxT("appicon"), wxBITMAP_TYPE_ICO_RESOURCE, coord, coord);
#else
#	error "What platform are you compiling for?"
#endif
}

MainFrame::MainFrame(fz::logger_interface &logger)
	: wxFrame(nullptr, wxID_ANY, _F("Administration interface - %s %s", fz::build_info::package_name, fz::build_info::version))
	, logger_(logger)
{
	wxTheApp->SetTopWindow(this);
	wxInitAllImageHandlers();

	wxIconBundle logos;
	for (wxCoord c: {16, 20, 24, 32, 48, 64, 128, 256}) {
		auto icon = get_logo(c);
		if (icon.IsOk())
			logos.AddIcon(icon);
	}

	SetIcons(logos);
	CreateStatusBar();
	SetStatusText(_S("Disconnected"));

	SetName(wxT("MainFrame"));
	PersistWindow(this);

	wxVBox(this, 0, 0) = wxSizerFlags(1).Expand() >>= book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto p) {
		administrator_ = wxPage<ServerAdministrator>(p, wxT("** Connected **"), false, logger_);

		wxPage(p, wxT("** Disconnected **")) | [&](auto p) {
			struct ConnectButton: wxButton
			{
				ConnectButton(wxWindow *parent, wxMenuItem *&m)
					: wxButton(parent, wxID_ANY)
					, m_(m)
				{}

				bool Enable(bool enable = true) override
				{
					if (enable) {
						if (m_)
							m_->Enable(true);
						SetLabel(_S("&Connect to Server..."));
					}
					else {
						if (m_)
							m_->Enable(false);
						SetLabel(_S("Connecting to Server..."));
					}

					GetParent()->Layout();

					return wxButton::Enable(enable);
				}

			private:
				wxMenuItem *&m_;
			};

			wxVBox(p, 0, 0) = {
				wxEmptySpace,
				wxSizerFlags(0).Center() >>= connect_button_ = new ConnectButton(p, connect_menu_),
				wxEmptySpace
			};
		};
	};

	connect_button_->Enable();
	connect_button_->Bind(wxEVT_BUTTON, &MainFrame::OnConnect, this);

	administrator_->Bind(ServerAdministrator::Event::Connection, &MainFrame::OnAdminConnection, this);
	administrator_->Bind(ServerAdministrator::Event::Disconnection, &MainFrame::OnAdminDisconnection, this);
	administrator_->Bind(ServerAdministrator::Event::Reconnection, &MainFrame::OnAdminReconnection, this);
	administrator_->Bind(ServerAdministrator::Event::ServerInfoUpdated, &MainFrame::OnServerInfoUpdated, this);

	tray_icon_ = std::make_unique<TrayIcon>(GetIcon(), *administrator_);

	book_->ChangeSelection(page::disconnected);

	wxMenu *menuServer = new wxMenu;
	connect_menu_ = menuServer->Append(ID_Connect, _S("&Connect...\tCtrl-H")
												 , _F("Connect to the administration port of a %s.", fz::build_info::package_name));

	disconnect_menu_ = menuServer->Append(ID_Disconnect, _S("&Disconnect...\tCtrl-D")
													   , _F("Disconnect from the %s.", fz::build_info::package_name));

	configure_menu_ = menuServer->Append(ID_Configure, _S("Con&figure...\tCtrl-F")
													 , _F("Configure the %s.", fz::build_info::package_name));

	network_menu_ = menuServer->Append(ID_ConfigureNetwork, _S("&FTP Network Configuration Wizard\tCtrl-N")
														  , _F("Use a wizard to configure the %s's FTP network connection.", fz::build_info::package_name));

	test_menu_ = menuServer->Append(ID_TestNetwork, _S("&Test FTP Network Configuration...\tCtrl-T")
												  , _F("A tool to test the %s's FTP network configuration.", fz::build_info::package_name));

	export_menu_ = menuServer->Append(ID_ExportConfig, _S("&Export configuration...\tCtrl-E")
													 , _F("Export the %s configuration to a file.", fz::build_info::package_name));

	import_menu_ = menuServer->Append(ID_ImportConfig, _S("&Import configuration...\tCtrl-I")
													 , _F("Import the %s configuration from a file.", fz::build_info::package_name));

#ifdef ENABLE_FZ_UPDATE_CHECKER
	menuServer->AppendSeparator();
	check_for_updates_menu_ = menuServer->Append(ID_Update, _S("Check for &updates...\tCtrl-U")
														  , _F("Check whether there are new versions of %s available.", fz::build_info::package_name));
#endif

	configure_menu_->Enable(false);
	disconnect_menu_->Enable(false);
	network_menu_->Enable(false);
	test_menu_->Enable(false);
	export_menu_->Enable(false);
	import_menu_->Enable(false);

	if (check_for_updates_menu_)
		check_for_updates_menu_->Enable(false);

	menuServer->AppendSeparator();
	menuServer->Append(wxID_EXIT);

	wxMenu *menuWindow = new wxMenu;
	menuWindow->AppendCheckItem(ID_StartMinimized, _S("Start minimi&zed"),
												   _S("Start the administration UI with its window minimized."))->Check(Settings()->start_minimized());

	enable_tray_icon_menu_ = menuWindow->AppendCheckItem(ID_EnableTrayIcon,
		_S("Enable &tray icon"),
		_S("Enable status icon in the system tray.")
	);
	enable_tray_icon_menu_->Check(Settings()->tray_icon_mode() != Settings()->tray_icon_disabled);

	minimize_to_tray_menu_ = menuWindow->AppendCheckItem(ID_MinimizeToTray,
		_S("Minimize to tra&y"),
		_S("Minimize the administration UI window into the system tray.")
	);
	minimize_to_tray_menu_->Check(Settings()->tray_icon_mode() == Settings()->minimize_to_tray);

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT);

	/*
	 * FIXME: to enable this, we need to implement some kind of notification system so that the rest of the UI can be updated with the reset-config
	menuHelp->AppendSeparator();
	menuHelp->Append(ID_ResetUIConfig, _S("Reset UI configura&tion"),
									   _S("Reset the UI configuration to default values"));
	*/
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuServer, wxS("&Server"));
	menuBar->Append(menuWindow, wxS("&Window"));
	menuBar->Append(menuHelp, wxS("&Help"));

	SetMenuBar( menuBar );

	Bind(wxEVT_MENU, &MainFrame::OnConnect, this, ID_Connect);
	Bind(wxEVT_MENU, &MainFrame::OnDisconnect, this, ID_Disconnect);
	Bind(wxEVT_MENU, &MainFrame::OnConfigure, this, ID_Configure);
	Bind(wxEVT_MENU, &MainFrame::OnConfigureNetwork, this, ID_ConfigureNetwork);
	Bind(wxEVT_MENU, &MainFrame::OnTestNetwork, this, ID_TestNetwork);
	Bind(wxEVT_MENU, &MainFrame::OnExportConfig, this, ID_ExportConfig);
	Bind(wxEVT_MENU, &MainFrame::OnImportConfig, this, ID_ImportConfig);

#ifdef ENABLE_FZ_UPDATE_CHECKER
	Bind(wxEVT_MENU, &MainFrame::OnCheckForUpdates, this, ID_Update);
#endif

	Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);

	Bind(wxEVT_MENU, &MainFrame::OnStartMinimized, this, ID_StartMinimized);
	Bind(wxEVT_MENU, &MainFrame::OnEnableTrayIcon, this, ID_EnableTrayIcon);
	Bind(wxEVT_MENU, &MainFrame::OnMinimizeToTray, this, ID_MinimizeToTray);

	Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
	Bind(wxEVT_MENU, &MainFrame::OnResetUIConfig, this, ID_ResetUIConfig);

	Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);

	enable_tray_icon(enable_tray_icon_menu_->IsChecked());

	if (enable_tray_icon_menu_->IsChecked())
		enable_minimize_to_tray(minimize_to_tray_menu_->IsChecked());
}

void MainFrame::TryToAutoConnect()
{
	if (auto info = Settings()->servers_info().get_autoconnect_server_info()) {
		connect_button_->Disable();
		administrator_->ConnectTo(*info, reconnect_timeout);
	}
}

void MainFrame::OnConnect(wxCommandEvent &)
{
	if (!connect_button_->IsEnabled())
		return;

	if (administrator_->IsConnected()) {
		administrator_->Disconnect();
	}

	connect_button_->Disable();

	wxPushDialog<ConnectionDialog>(this, wxT("Connection")) | [this] (ConnectionDialog &conn_diag) {
		conn_diag.SetServersInfo(Settings()->servers_info());

		if (conn_diag.ShowModal() != wxID_OK) {
			connect_button_->Enable();
			connect_button_->SetFocus();
			return;
		}

		Settings().Save();
		administrator_->ConnectTo(conn_diag.GetSelectedServerInfo(), reconnect_timeout);
	};
}

void MainFrame::OnConfigure(wxCommandEvent &)
{
	administrator_->ConfigureServer();
}

void MainFrame::OnConfigureNetwork(wxCommandEvent &)
{
	administrator_->ConfigureNetwork();
}

void MainFrame::OnTestNetwork(wxCommandEvent &)
{
	administrator_->TestNetwork();
}

void MainFrame::OnExportConfig(wxCommandEvent &)
{
	administrator_->ExportConfig();
}

void MainFrame::OnImportConfig(wxCommandEvent &)
{
	administrator_->ImportConfig();
}

#ifdef ENABLE_FZ_UPDATE_CHECKER
void MainFrame::OnCheckForUpdates(wxCommandEvent &)
{
	administrator_->CheckForUpdates();
}
#endif

void MainFrame::OnDisconnect(wxCommandEvent &)
{
	administrator_->Disconnect();
}

void MainFrame::OnClose(wxCloseEvent &event)
{
	if (!event.CanVeto() || !administrator_ || !administrator_->IsConnected()) {
		Destroy();
		Settings().Save();
		return;
	}

	close_on_disconnect_ = true;
	administrator_->Disconnect();
	event.Veto();
}

void MainFrame::OnExit(wxCommandEvent&)
{
	Close();
}

void MainFrame::OnStartMinimized(wxCommandEvent &ev)
{
	Settings()->start_minimized(ev.IsChecked());
	Settings().Save();
}

void MainFrame::OnEnableTrayIcon(wxCommandEvent &ev)
{
	enable_tray_icon(ev.IsChecked());

	Settings().Save();
}

void MainFrame::OnMinimizeToTray(wxCommandEvent &ev)
{
	enable_minimize_to_tray(ev.IsChecked());
	Settings().Save();
}

void MainFrame::OnAbout(wxCommandEvent &)
{
	wxAboutDialogInfo info;
	info.SetIcon(GetIcon());
	info.SetName(fz::to_wxString(fz::build_info::package_name));
	info.SetVersion(fz::to_wxString(std::wstring(fz::build_info::version)));
	info.SetDescription(_F("Administration interface for the %s", fz::build_info::package_name));
	info.SetCopyright(fz::to_wxString(fz::build_info::copyright));
	wxAboutBox(info);
}

void MainFrame::OnResetUIConfig(wxCommandEvent &)
{
	Settings().Reset();
}

void MainFrame::OnAdminConnection(ServerAdministrator::Event &e)
{
	if (!e.error) {
		book_->ChangeSelection(page::connected);
		//administrator_->SetFocus();
		configure_menu_->Enable(true);
		disconnect_menu_->Enable(true);
		network_menu_->Enable(true);
		test_menu_->Enable(true);
		export_menu_->Enable(true);
		import_menu_->Enable(true);

		if (check_for_updates_menu_)
			check_for_updates_menu_->Enable(true);

		SetStatusText(_F("Connected to %s", fz::to_wxString(administrator_->GetServerInfo().host)));
	}
	else {
		administrator_->Disconnect();

		auto reason =
			e.error == EACCES
				? _S("The password is not valid.")
				: _S("Error: %s (%d)");

		wxMsg::Error(_S("Couldn't connect to the server."))
			.Ext(reason, fz::socket_error_description(e.error), e.error);
	}
}

void MainFrame::OnAdminDisconnection(ServerAdministrator::Event &e)
{
	if (this->IsBeingDeleted())
		return;

	book_->ChangeSelection(page::disconnected);
	configure_menu_->Enable(false);
	disconnect_menu_->Enable(false);
	network_menu_->Enable(false);
	test_menu_->Enable(false);
	export_menu_->Enable(false);
	import_menu_->Enable(false);

	if (check_for_updates_menu_)
		check_for_updates_menu_->Enable(false);

	connect_menu_->Enable(true);
	connect_button_->Enable(true);

	SetStatusText(_S("Disconnected"));

	if (close_on_disconnect_)
		Close(true);
	else
	if (e.error)
		wxMsg::Error(_S("Lost connection with the server. Error: %s (%d)"), fz::socket_error_description(e.error), e.error);
	else
	if (e.server_initiated_disconnection)
		wxMsg::Error(_S("Remote server closed the connection."));
}

void MainFrame::OnAdminReconnection(ServerAdministrator::Event &)
{
	book_->ChangeSelection(page::connected);
	configure_menu_->Enable(false);
	disconnect_menu_->Enable(true);
	connect_menu_->Enable(false);
	network_menu_->Enable(false);
	test_menu_->Enable(false);
	export_menu_->Enable(false);
	import_menu_->Enable(false);

	if (check_for_updates_menu_)
		check_for_updates_menu_->Enable(false);

	SetStatusText(_F("Reconnecting to %s...", fz::to_wxString(administrator_->GetServerInfo().host)));
}

void MainFrame::OnServerInfoUpdated(ServerAdministrator::Event &)
{
	const auto& new_info = administrator_->GetServerInfo();

	for (auto &info: Settings()->servers_info()) {
		if (new_info.host == info.host && new_info.port == info.port) {
			info = new_info;
			Settings().Save();
			break;
		}
	}
}

void MainFrame::enable_tray_icon(bool enable)
{
	if (enable) {
		minimize_to_tray_menu_->Enable(true);
		Settings()->tray_icon_mode(Settings()->tray_icon_enabled);
		tray_icon_->SetFrame(this);
	}
	else {
		minimize_to_tray_menu_->Enable(false);
		minimize_to_tray_menu_->Check(false);
		Settings()->tray_icon_mode(Settings()->tray_icon_disabled);
		tray_icon_->SetFrame(nullptr);
	}
}

void MainFrame::enable_minimize_to_tray(bool enable)
{
	if (tray_icon_) {
		Settings()->tray_icon_mode(enable ? Settings()->minimize_to_tray : Settings()->tray_icon_enabled);
		tray_icon_->EnableFrameIconization(enable);
	}
}

wxIcon MainFrame::GetIcon()
{
	auto w = wxSystemSettings::GetMetric(wxSYS_ICON_X);
	auto h = wxSystemSettings::GetMetric(wxSYS_ICON_Y);
	return GetIcons().GetIcon({w, h}, wxIconBundle::FALLBACK_NEAREST_LARGER);
}
