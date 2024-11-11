
#include <libfilezilla/glue/wx.hpp>

#include <wx/checkbox.h>
#include <wx/valgen.h>
#include <wx/scrolwin.h>
#include <wx/statline.h>
#include <wx/radiobut.h>
#include <wx/hyperlink.h>
#include <wx/textctrl.h>

#include "networkconfigwizard.hpp"
#include "helpers.hpp"

#include "integraleditor.hpp"
#include "textvalidatorex.hpp"
#include "ftptester.hpp"

#include "../filezilla/port_randomizer.hpp"
#include "../filezilla/build_info.hpp"

NetworkConfigWizard::NetworkConfigWizard(wxWindow *parent, fz::thread_pool &pool, fz::event_loop &loop, fz::tls_system_trust_store *trust_store, fz::logger_interface &logger, const wxString &title)
	: wxDialogEx(parent, wxID_ANY, title, wxNullBitmap, wxDefaultPosition, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxPage(this, _S("FTP Data connection modes")) |[&] (auto p) {
		wxVBox(p, 0) = {
			wxWText(p, _S("FTP supports two ways to establish data connections for transfers: active and passive mode.")),
			wxWText(p, _S("Passive mode is the recommended mode most clients default to.")),
			wxWText(p, _F("In Passive mode clients ask %s which server port to connect to.", fz::build_info::package_name)),
			wxWText(p, _F("The wizard helps you configure %s and set up your router or firewall to support passive mode on your server.", fz::build_info::package_name)),
			wxWText(p, _S("At the end of the configuration process the wizard suggests you test your configurations, providing a link to our online server tester.")),
			wxWText(p, _S("Note: Active mode does not require server-side configuration."))
		};
	};

	wxPage(this, _S("Setting up Passive mode port range")) | [&](auto p) {
		wxVBox(p, 0) = {
			wxWText(p, _F("You need to set the range of ports %s uses for passive mode data connections.", fz::build_info::package_name)),
			wxWText(p, _S("Set the range greater than the number of transfers you want to serve in a 4-minutes time period.")),

			use_custom_port_range_ctrl_ = new wxRadioButton(p, wxID_ANY, _S("&Use custom port range:"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP),
			wxVBox(p, {wxLEFT, wxDefaultPadding*2}) = selected_port_range_content_ = new wxPanel(p, nullID) | [&](auto p) {
				wxVBox(p, 0) = {
					wxLabel(p, _F("&From: (suggested is %d)", fz::port_randomizer::min_ephemeral_value)),
					min_port_range_ctrl_ = wxCreate<IntegralEditor>(p),

					wxLabel(p, _F("&To: (suggested is %d)", fz::port_randomizer::max_ephemeral_value)),
					max_port_range_ctrl_ = wxCreate<IntegralEditor>(p),

					wxLabel(p, _S("Configure your NAT routers to forward the specified range.")),
					wxLabel(p, _S("Open the same TCP ports on your firewalls as well."))
				};
			},

			use_ports_from_os_ctrl_ = new wxRadioButton(p, wxID_ANY, _S("Use &any available port.")),
			ports_from_os_explanation_ctrl_ = new wxPanel(p) | [&] (auto p) {
				wxVBox(p, {wxLEFT, wxDefaultPadding*2}) = {
					wxLabel(p, _S("Configure your NAT routers to forward all TCP ports.")),
					wxLabel(p, _S("Open all the TCP ports on your firewalls as well."))
				};
			},
		};

		use_custom_port_range_ctrl_->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent &) {
			ftp_options_.sessions().pasv.port_range.emplace();
			select_port_range();
		});

		use_ports_from_os_ctrl_->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent &) {
			ftp_options_.sessions().pasv.port_range.reset();
			select_port_range();
		});
	};

	wxPage(this, _S("Passive mode: setting public IP or hostname")) |[&](auto p) {
		wxVBox(p, 0) = {
			wxWText(p, _F("To properly support the passive mode, if %1$s is connected to the external network via a NAT device, "
						  "it's necessary to specify which is the external IP address or hostname %1$s can be reached at.", fz::build_info::package_name)),
			wxWText(p, _S("&Enter the public IP or hostname (if you leave it empty FileZilla Server uses the local IP):")),

			wxHBox(p, 0) = {
				wxSizerFlags(1) >>= host_override_ctrl_ = new wxTextCtrl(p, nullID),
				fx::RetrievePublicIpButton(p, *host_override_ctrl_, fx::ipv_type::ipv4, get_public_ip_func_)
			},

			disallow_host_override_for_local_peers_ctrl_ = new wxCheckBox(p, wxID_ANY, _S("&Use local IP for local connections (recommended).")),
		};

		Bind(wxEVT_WIZARD_PAGE_CHANGING, [&, page = p->GetParent()](wxWizardEvent &ev) {
			if (ev.GetPage() != page) {
				ev.Skip();
				return;
			}

			bool forward = ev.GetDirection();
			if (!forward) {
				ev.Skip();
				return;
			}

			const wxString &val = host_override_ctrl_->GetValue();
			bool is_valid = !host_override_ctrl_->IsModified() || fx::ValidatePassiveModeHostMsg(val);

			if (is_valid) {
				host_override_ctrl_->SetModified(false);
				ftp_options_.sessions().pasv.host_override = fz::to_native(val);
				ev.Skip();
			}
			else {
				host_override_ctrl_->SetFocusFromKbd();
				ev.Veto();
			}
		});
	};

	wxPage(this, _S("Network Configuration settings")) |[&] (auto p) {
		wxVBox(p, 0) = {
			wxLabel(p, _S("These are the choices you made:")),
			{ wxSizerFlags(0).Border(wxUP, 0), wxVBox(p) = {1, wxGBox(p, 2, {1}, {}, wxGBoxDefaultGap, wxALIGN_TOP) = {
			   wxLabel(p, _S("Port range:")),
			   summary_ports_ctrl_ =  new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_WORDWRAP | wxTE_MULTILINE),

			   wxLabel(p, _S("External IP or hostname:")),
			   summary_host_ctrl_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_WORDWRAP | wxTE_MULTILINE),

			   wxLabel(p, _S("Use internal IP for local connections:")),
			   summary_local_connections_ctrl_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_WORDWRAP | wxTE_MULTILINE),
			}}},

			wxLabel(p, _S("Remember:")),
			wxVBox(p, {wxLEFT, wxDefaultPadding*2}) = wxGBox(p, 2, {1}, {}, wxGBoxDefaultGap, wxALIGN_TOP) = {
				wxLabel(p, _S("1)")),
				wxWText(p, _F("if %s is connected to the external network via a NAT device, the chosen ports must be all forwarded;", fz::build_info::package_name)),

				wxLabel(p, _S("2)")),
				wxWText(p, _F("if %s is protected by a firewall, the choosen ports must be all open.", fz::build_info::package_name)),
			},

			wxWText(p, _S("In the next page you will be prompted to test this configuration."))
		};

		wxTransferDataToWindow(p, [this] {
			auto &range = ftp_options_.sessions().pasv.port_range;
			wxString summary_ports_value;
			wxString summary_host_value;
			wxString summary_local_connections_value;

			if (range.has_value())
				summary_ports_value = _F("custom, from %d to %d.", range->min, range->max);
			else
				summary_ports_value = _S("the Operating System will choose the first available port from the full set.");

			if (!host_override_ctrl_->IsEmpty())
				summary_host_value = host_override_ctrl_->GetValue();
			else
				summary_host_value = _S("No public IP, only local IP.");

			if (disallow_host_override_for_local_peers_ctrl_->GetValue())
				summary_local_connections_value = _S("Yes.");
			else
				summary_local_connections_value = _S("No.");

			summary_ports_ctrl_->ChangeValue(summary_ports_value);
			summary_host_ctrl_->ChangeValue(summary_host_value);
			summary_local_connections_ctrl_->ChangeValue(summary_local_connections_value);

			return true;
		});
	};

	wxPage(this, _S("Test the settings")) | [&](auto p) {
		wxString finish = _S("Finish");
		wxString back = _S("Back");

		if (auto but = wxDynamicCast(FindWindowById(wxID_BACKWARD, this), wxButton)) {
			back = but->GetLabelText();
		}

		wxVBox(p) = {
			wxWText(p, _F("This page allows you to test %s's network configuration over the internet using our online FTP Server Tester (https://ftptest.net).", fz::build_info::package_name)),
			wxWText(p, _F("To modify your configuration at any time, click the [%s] button.", back)),
			wxWText(p, _F("Once you're satisfied with the configuration, click the [%s] button to save your settings and exit the wizard.", finish)),

			wxSizerFlags(1) >>= ftp_tester_ctrl_ = new FtpTester(p, pool, loop, trust_store, logger)
		};

		Bind(wxEVT_WIZARD_PAGE_CHANGING, [&, page = p->GetParent()](wxWizardEvent &ev) {
			if (ev.GetPage() != page) {
				ev.Skip();
				return;
			}

			bool back = !ev.GetDirection();

			if (back) {
				ftp_tester_ctrl_->Stop();
				return;
			}

			wxString warning;

			if (ftp_tester_ctrl_->IsRunning()) {
				warning = _S("The test is still running!");
			}
			else
			switch (int(ftp_tester_ctrl_->GetLastFinishReasonType())) {
				case FtpTester::Event::Warning:
					warning = _S("The test finished with warnings.");
					break;

				case FtpTester::Event::Error:
					warning = _S("The test failed.");
					break;

				case FtpTester::Event::Success:
					/* It's ok */
					break;

				default:
					warning = _S("The test did not yet terminate.");
					break;
			}

			if (!warning.empty()) {
				bool ok = wxMsg::WarningConfirm(warning).Ext(_S("Do you really want to finish the wizard and save the configuration?")) == wxID_YES;
				if (!ok) {
					ev.Veto();
					return;
				}
			}
		});
	};
}

void NetworkConfigWizard::SetFtpOptions(const fz::ftp::server::options &opts)
{
	ftp_options_ = opts;

	ftp_tester_ctrl_->SetFtpOptions(&ftp_options_);

	select_port_range();

	host_override_ctrl_->SetValue(fz::to_wxString(ftp_options_.sessions().pasv.host_override));
	disallow_host_override_for_local_peers_ctrl_->SetValidator(wxGenericValidator(&ftp_options_.sessions().pasv.do_not_override_host_if_peer_is_local));
}

const fz::ftp::server::options &NetworkConfigWizard::GetFtpOptions() const
{
	return ftp_options_;
}

void NetworkConfigWizard::SetGetPublicIpFunc(GetPublicIpFunc func)
{
	get_public_ip_func_ = func;

	ftp_tester_ctrl_->SetGetPublicIpFunc(std::move(func));
}

void NetworkConfigWizard::SetCreateFtpTestEnvironmentFunc(CreateFtpTestEnvironmentFunc func)
{
	ftp_tester_ctrl_->SetCreateFtpTestEnvironmentFunc(std::move(func));
}

bool NetworkConfigWizard::Run()
{
	return RunWizard(GetFirstPage(this));
}

void NetworkConfigWizard::select_port_range()
{
	auto &range = ftp_options_.sessions().pasv.port_range;

	if (range.has_value()) {
		use_custom_port_range_ctrl_->SetValue(true);
		selected_port_range_content_->Enable();
		min_port_range_ctrl_->SetRef(range->min, 1, 65535);

		max_port_range_ctrl_->SetRef(range->max, 1, 65535);

		ports_from_os_explanation_ctrl_->Disable();
	}
	else {
		use_ports_from_os_ctrl_->SetValue(true);

		selected_port_range_content_->Disable();
		min_port_range_ctrl_->SetRef(nullptr);

		max_port_range_ctrl_->SetRef(nullptr);

		ports_from_os_explanation_ctrl_->Enable();
	}
}
