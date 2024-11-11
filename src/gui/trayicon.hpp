#ifndef TRAYICON_HPP
#define TRAYICON_HPP

#include <wx/taskbar.h>
#include <wx/icon.h>

#include "serveradministrator.hpp"

class TrayIcon
{
public:
	TrayIcon(const wxIcon &icon, ServerAdministrator &sa);
	virtual ~TrayIcon();

	void SetFrame(wxFrame *w);
	void EnableFrameIconization(bool enable);

private:
	void OnServerStatusChanged(const ServerAdministrator::Event &ev);
	void OnIconize(wxIconizeEvent &ev);

	wxIcon without_active_sessions_icon_;
	wxIcon with_active_sessions_icon_;
	wxIcon disconnected_icon_;

	ServerAdministrator &server_admin_;
	wxTaskBarIcon taskbar_icon_;
	wxFrame *frame_{};
	bool iconization_enabled_{};
};

#endif // TRAYICON_HPP
