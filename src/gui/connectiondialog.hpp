#ifndef WXFZFTPCONNECTIONDIALOG_HPP
#define WXFZFTPCONNECTIONDIALOG_HPP

#include <wx/propdlg.h>

#include "dialogex.hpp"
#include "serveradministrator.hpp"

class IntegralEditor;

class ConnectionDialog: public wxDialogEx<wxPropertySheetDialog>
{
public:
	struct servers_info: std::vector<ServerAdministrator::server_info>
	{
		using vector::vector;

		const ServerAdministrator::server_info *get_autoconnect_server_info() const;
		const ServerAdministrator::server_info *get_last_connected_server_info() const;

	private:
		friend ConnectionDialog;
		friend fz::serialization::access;

		template <typename Archive>
		void serialize(Archive &ar)
		{
			using namespace fz::serialization;

			ar(
				nvp(static_cast<vector&>(*this), "", "server"),
				optional_nvp(autoconnect_to_server, "autoconnect_to_server"),
				optional_nvp(last_connected_server, "last_connected_server")
			);
		}

		std::string autoconnect_to_server{};
		std::string last_connected_server{};
	};

	ConnectionDialog();

	bool Create(wxWindow *parent,
				const wxString &title,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER,
				const wxString& name = wxS("ConnectionDialog"));

	void SetServersInfo(servers_info &infos);
	const servers_info::value_type &GetSelectedServerInfo() const;

private:
	wxBookCtrlBase* CreateBookCtrl() override;
	void AddBookCtrl(wxSizer *sizer) override;

	bool TransferDataFromWindow() override;
	bool TransferDataToWindow() override;

private:
	servers_info *infos_{};
	servers_info::value_type selected_info_{};
	bool save_password_{};
	bool autoconnect_{};
	wxTextCtrl *host_ctrl_{};
	IntegralEditor *port_ctrl_{};
};

FZ_SERIALIZATION_SPECIALIZE_ALL(ConnectionDialog::servers_info, unversioned_member_serialize)

#endif // WXFZFTPCONNECTIONDIALOG_HPP
