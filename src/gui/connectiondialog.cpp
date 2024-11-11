#include <libfilezilla/glue/wx.hpp>

#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/bookctrl.h>
#include <wx/panel.h>
#include <wx/valtext.h>
#include <wx/checkbox.h>
#include <wx/valgen.h>
#include <wx/statline.h>
#include <wx/simplebook.h>

#include "connectiondialog.hpp"
#include "locale.hpp"

#include "textvalidatorex.hpp"
#include "integraleditor.hpp"

#include "helpers.hpp"

ConnectionDialog::ConnectionDialog()
{
}

bool ConnectionDialog::Create(wxWindow *parent, const wxString &title, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxDialogEx::Create(parent, winid, title, pos, size, style, name))
		return false;

	GetBookCtrl() | [&](auto p) {
		wxPage(p, _S("Direct")) | [&](auto p) {
			wxCheckBox *save_password_ctrl{};
			wxCheckBox *autoconnect_ctrl{};

			wxVBox(p, 0) = {
				{ 0, wxLabel(p, _S("&Host:")) },
				{ 0, host_ctrl_ = new wxTextCtrl(p, wxID_ANY) | [&] (auto p) {
					p->SetValidator(TextValidatorEx(wxFILTER_NONE, &selected_info_.host, field_must_not_be_empty(_S("Host"))));
				}},

				{ 0, wxLabel(p, _S("&Port:")) },
				{ 0, port_ctrl_ = wxCreate<IntegralEditor>(p, wxEmptyString, 1, 0) | [&] (auto p) {
					p->SetRef(selected_info_.port, 1, 65535);
				}},

				{ 0, wxLabel(p, _S("Pass&word:")) },
				{ 0, new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD) | [&] (auto p) {
					p->SetValidator(TextValidatorEx(wxFILTER_NONE, &selected_info_.password));
				}},

				{ 0, save_password_ctrl = new wxCheckBox(p, wxID_ANY, _S("Save &the password")) | [&](auto p) {
					p->SetValidator(wxGenericValidator(&save_password_));
				}},

				{ 0, autoconnect_ctrl = new wxCheckBox(p, wxID_ANY, _S("Automatically &connect to this server at startup")) | [&](auto p) {
					p->SetValidator(wxGenericValidator(&autoconnect_));
				}},

				#if defined(ENABLE_ADMIN_DEBUG) && ENABLE_ADMIN_DEBUG
					{ 0, wxLabel(p, _S("&Use TLS:")) },
					{ 0, wxCreate<IntegralEditor>(p) | [&] (auto p) {
						p->SetRef(selected_info_.use_tls, false, true);
					}}
				#endif
			};

			save_password_ctrl->Bind(wxEVT_CHECKBOX, [autoconnect_ctrl](wxCommandEvent &ev) {
				autoconnect_ctrl->Enable(ev.IsChecked());
				if (!ev.IsChecked())
					autoconnect_ctrl->SetValue(false);
			});

			host_ctrl_->Bind(wxEVT_TEXT, [&](wxCommandEvent &ev) {
				ev.Skip();

				if (!infos_)
					return;

				wxArrayString ports;
				for (const auto &i: *infos_) {
					const auto &host = fz::to_utf8(ev.GetString());

					if (i.host == host)
						ports.push_back(fz::to_wxString(fz::to_wstring(i.port)));

					port_ctrl_->AutoComplete(ports);
				}
			});
		};
	};

	CreateButtons(wxOK | wxCANCEL);

	LayoutDialog();

	return true;
}

void ConnectionDialog::SetServersInfo(servers_info &infos)
{
	infos_ = &infos;

	if (!infos.empty()) {
		if (auto i = infos.get_last_connected_server_info())
			selected_info_ = *i;
		else
			selected_info_ = infos.front();
	}
	else {
		selected_info_ = {};
	}

	save_password_ = !selected_info_.password.empty();
	autoconnect_ = infos_->autoconnect_to_server == selected_info_.name;

	wxArrayString hosts;
	for (const auto &i: infos)
		hosts.push_back(fz::to_wxString(i.host));

	host_ctrl_->AutoComplete(hosts);
}

const ConnectionDialog::servers_info::value_type &ConnectionDialog::GetSelectedServerInfo() const
{
	return selected_info_;
}

wxBookCtrlBase *ConnectionDialog::CreateBookCtrl()
{
	return wxCreate<wxNavigationEnabled<wxSimplebook>>(this);
}

void ConnectionDialog::AddBookCtrl(wxSizer *sizer)
{
	sizer->Add(GetBookCtrl(), wxSizerFlags(1).Expand().Border(wxALL, wxDlg2Px(this, wxDefaultPadding)));
}

bool ConnectionDialog::TransferDataFromWindow()
{
	if (!wxDialogEx::TransferDataFromWindow())
		return false;

	// TODO: in the future, the name will have to be explictly settable by the user.
	selected_info_.name = fz::join_host_and_port(selected_info_.host, selected_info_.port);
	selected_info_.fingerprint = {};

	if (infos_) {
		auto it = std::find_if(infos_->begin(), infos_->end(), [&](const auto &i) {
			return i.host == selected_info_.host && i.port == selected_info_.port;
		});

		if (it != infos_->end()) {
			it->password = selected_info_.password;
			// TODO: the following line will have to be removed once the name will be made user-settable.
			it->name = selected_info_.name;
			selected_info_.fingerprint = it->fingerprint;
		}
		else {
			infos_->push_back(selected_info_);
			it = std::prev(infos_->end());
		}

		if (!save_password_)
			it->password = {};

		if (autoconnect_)
			infos_->autoconnect_to_server = selected_info_.name;
		else
			infos_->autoconnect_to_server.clear();

		infos_->last_connected_server = selected_info_.name;
	}

	return true;
}

bool ConnectionDialog::TransferDataToWindow()
{
	return wxDialogEx::TransferDataToWindow();
}

const ServerAdministrator::server_info *ConnectionDialog::servers_info::get_autoconnect_server_info() const
{
	if (autoconnect_to_server.empty())
		return nullptr;

	auto it = std::find_if(cbegin(), cend(), [this](const ServerAdministrator::server_info &info) {
		return info.name == autoconnect_to_server;
	});

	if (it == cend())
		return nullptr;

	return std::addressof(*it);
}

const ServerAdministrator::server_info *ConnectionDialog::servers_info::get_last_connected_server_info() const
{
	if (last_connected_server.empty())
		return nullptr;

	auto it = std::find_if(cbegin(), cend(), [this](const ServerAdministrator::server_info &info) {
		return info.name == last_connected_server;
	});

	if (it == cend())
		return nullptr;

	return std::addressof(*it);
}
