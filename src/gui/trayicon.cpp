#include <libfilezilla/glue/wx.hpp>

#include <wx/frame.h>
#include <wx/icon.h>
#include <wx/bitmap.h>
#include <wx/image.h>

#include "trayicon.hpp"
#include "locale.hpp"

#include <iostream>

TrayIcon::TrayIcon(const wxIcon &icon, ServerAdministrator &sa)
	: without_active_sessions_icon_(icon)
	, server_admin_(sa)
{
	if (without_active_sessions_icon_.IsOk()) {
		auto img = [&]() -> wxImage {
			wxBitmap bmp;
			bmp.CopyFromIcon(without_active_sessions_icon_);
			return bmp.ConvertToImage();
		}();

		// Swap red and green
		for (auto data = img.GetData(), end = data + img.GetHeight()*img.GetWidth()*3; data != end; data += 3)
			std::swap(data[0], data[1]);

		// In the mask too
		if (img.HasMask()) {
			auto mr = img.GetMaskRed();
			auto mg = img.GetMaskGreen();
			auto mb = img.GetMaskBlue();

			img.SetMaskColour(mg, mr, mb);
		}

		with_active_sessions_icon_.CopyFromBitmap(wxBitmap(img));

		disconnected_icon_.CopyFromBitmap(wxBitmap(img.ConvertToGreyscale()));
	}

#ifdef __WXMSW__
	auto evt = wxEVT_TASKBAR_LEFT_UP;
#else
	auto evt = wxEVT_TASKBAR_LEFT_DCLICK;
#endif

	taskbar_icon_.Bind(evt, [&](wxTaskBarIconEvent &) {
		if (!frame_)
			return;

		frame_->Show();
		frame_->Restore();
		frame_->Raise();
	});
}

TrayIcon::~TrayIcon()
{
	SetFrame(nullptr);
}

void TrayIcon::SetFrame(wxFrame *w)
{
	if (w == frame_)
		return;

	if (!w && frame_) {
		frame_->Unbind(wxEVT_ICONIZE, &TrayIcon::OnIconize, this);
		server_admin_.Unbind(ServerAdministrator::Event::ServerStatusChanged, &TrayIcon::OnServerStatusChanged, this);
		iconization_enabled_ = false;
	}
	else
	if (w && !frame_) {
		server_admin_.Bind(ServerAdministrator::Event::ServerStatusChanged, &TrayIcon::OnServerStatusChanged, this);
		w->Bind(wxEVT_ICONIZE, &TrayIcon::OnIconize, this);
	}

	frame_ = w;

	if (frame_)
		OnServerStatusChanged(ServerAdministrator::Event::ServerStatusChanged());
	else
		taskbar_icon_.RemoveIcon();
}

void TrayIcon::EnableFrameIconization(bool enable)
{
	iconization_enabled_ = enable;
}

void TrayIcon::OnServerStatusChanged(const ServerAdministrator::Event &)
{
	const auto &server_status = server_admin_.GetServerStatus();

	wxString tooltip;
	wxIcon *icon{};

	if (server_status.num_of_active_sessions < 0) {
		tooltip = _S("Disconnected from server.");
		icon = &disconnected_icon_;
	}
	else {
		tooltip = _F("Active sessions: %d", int(server_status.num_of_active_sessions));

		if (server_status.num_of_active_sessions == 0)
			icon = &without_active_sessions_icon_;
		else
			icon = &with_active_sessions_icon_;
	}

	if (icon->IsOk())
		taskbar_icon_.SetIcon(*icon, tooltip);
}

void TrayIcon::OnIconize(wxIconizeEvent &ev)
{
	ev.Skip();

	if (!frame_ || !iconization_enabled_)
		return;

#ifdef __WXMSW__
	frame_->Show(!ev.IsIconized());
#endif
}
