#ifndef NETWORKCONFIGWIZARD_HPP
#define NETWORKCONFIGWIZARD_HPP

#include <functional>

#include <wx/wizard.h>
#include "dialogex.hpp"

#include "../filezilla/ftp/server.hpp"
#include "ftptester.hpp"

class wxCheckBox;
class IntegralEditor;
class wxTextCtrl;
class wxRadioButton;
class wxStaticText;

class NetworkConfigWizard: public wxDialogEx<wxWizard>
{
public:
	using GetPublicIpFunc = FtpTester::GetPublicIpFunc;
	using CreateFtpTestEnvironmentFunc = FtpTester::CreateFtpTestEnvironmentFunc;

	NetworkConfigWizard(wxWindow *parent, fz::thread_pool &pool, fz::event_loop &loop, fz::tls_system_trust_store *trust_store = nullptr, fz::logger_interface &logger = fz::get_null_logger(), const wxString &title = wxEmptyString);

	void SetFtpOptions(const fz::ftp::server::options &opts);
	const fz::ftp::server::options &GetFtpOptions() const;

	void SetGetPublicIpFunc(GetPublicIpFunc func);
	void SetCreateFtpTestEnvironmentFunc(CreateFtpTestEnvironmentFunc func);

	bool Run();

private:
	fz::ftp::server::options ftp_options_;

	wxRadioButton *use_custom_port_range_ctrl_{};
	wxRadioButton *use_ports_from_os_ctrl_{};
	IntegralEditor *min_port_range_ctrl_{};
	IntegralEditor *max_port_range_ctrl_{};
	wxTextCtrl *host_override_ctrl_{};
	wxCheckBox *disallow_host_override_for_local_peers_ctrl_{};
	wxWindow *ports_from_os_explanation_ctrl_{};
	wxWindow *selected_port_range_content_{};
	wxTextCtrl *summary_ports_ctrl_{};
	wxTextCtrl *summary_host_ctrl_{};
	wxTextCtrl *summary_local_connections_ctrl_{};
	FtpTester *ftp_tester_ctrl_{};

	GetPublicIpFunc get_public_ip_func_;

	void select_port_range();
};

#endif // NETWORKCONFIGWIZARD_HPP
