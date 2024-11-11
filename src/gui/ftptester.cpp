#include <libfilezilla/glue/wx.hpp>

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/hostname_lookup.hpp>

#include <wx/button.h>
#include <wx/radiobut.h>
#include <wx/choicebk.h>
#include <wx/notebook.h>
#include <wx/headerctrl.h>

#include "../filezilla/http/client.hpp"
#include "../filezilla/util/proof_of_work.hpp"

#include "ftptester.hpp"
#include "integraleditor.hpp"

#include "helpers.hpp"

class FtpTester::worker
{
public:
	worker(FtpTester &ftp_tester, std::vector<std::string> hosts, std::vector<std::string> public_ips, std::string username, std::string password, std::string port, std::string protocol)
		: ftp_tester_(ftp_tester)
		, shared_context_(this)
		, http_(ftp_tester.pool_, ftp_tester.loop_, ftp_tester.logger_, fz::http::client::options()
			.follow_redirects(true)
			.trust_store(ftp_tester.trust_store_)
			.default_timeout(fz::duration::from_seconds(60))
		)
		, hosts_(hosts)
		, public_ips_(public_ips)
		, username_(username)
		, password_(password)
		, port_(port)
		, protocol_(protocol)
	{
		ftp_tester_.pool_.spawn([&]{do_test(shared_context_);}).detach();
	}

	std::size_t num_or_remaining_tests() const
	{
		return num_of_remaining_tests_;
	}

	~worker()
	{
		shared_context_.stop_sharing();
	}

private:
	static void do_test(fz::shared_context<worker*> c);

	FtpTester &ftp_tester_;
	fz::shared_context<worker*> shared_context_;
	fz::http::client http_;

	std::vector<std::string> hosts_;
	std::vector<std::string> public_ips_;
	std::string username_;
	std::string password_;
	std::string port_;
	std::string protocol_;

	std::atomic_size_t num_of_remaining_tests_{};
};

FtpTester::FtpTester(wxWindow *parent, fz::thread_pool &pool, fz::event_loop &loop, fz::tls_system_trust_store *trust_store, fz::logger_interface &logger)
	: wxPanel(parent, nullID, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER, wxS("FtpTester"))
	, pool_(pool)
	, loop_(loop)
	, trust_store_(trust_store)
	, logger_(logger, "FtpTester")
{
	static const wxString tls_choices[2] = {
		_S("Explicit"),
		_S("Implicit")
	};

	wxButton *retrieve_ctrl{};

	wxSize extent = wxTextExtent(66, 6, this, wxFONTFAMILY_UNKNOWN, {wxSYS_VSCROLL_X});

	wxVBox(this, 0) = {
		wxStaticVBox(this, _S("Test parameters")) = {
			wxGBox(this, 2, {1})  = {
				wxLabel(this, _S("Host(s):")), wxSizerFlags(1) >>= wxHBox(this, 0) = {
					wxSizerFlags(1) >>= host_ctrl_ = new wxTextCtrl(this, nullID),
					retrieve_ctrl = fx::RetrievePublicIpButton(this, *host_ctrl_, fx::ipv_type::both, get_public_ip_func_)
				},
				wxLabel(this, _S("FTP Port:")), wxSizerFlags(1) >>= port_ctrl_ = wxCreate<IntegralEditor>(this, wxEmptyString, 1, 0),
				wxLabel(this, _S("TLS mode:")), tls_choices_ctrl_ = new wxChoice(this, nullID, wxDefaultPosition, wxDefaultSize, 2, tls_choices)
			},
		},

		start_stop_ctrl_ = new wxButton(this, nullID, _S("Start the test")),

		wxSizerFlags(1) >>= wxCreate<wxNotebook>(this, nullID) | [&](auto p) {
			wxSizerFlags(1) >>= log_ctrl_ = wxPage<wxTextCtrl>(p, _S("Test log"), true, nullID, wxEmptyString, wxDefaultPosition, extent, wxTE_READONLY | wxTE_RICH2 | wxTE_MULTILINE);
			wxSizerFlags(1) >>= results_ctrl_ = wxPage<wxTextCtrl>(p, _S("Test results"), false, nullID, _S("Test not started yet."), wxDefaultPosition, extent, wxTE_READONLY | wxTE_RICH2 | wxTE_MULTILINE);
		}
	};

	port_ctrl_->SetRef(21, 1, 65535);
	tls_choices_ctrl_->SetSelection(0);

	start_stop_ctrl_->Bind(wxEVT_BUTTON, [=](auto &) {
		if (worker_) {
			Stop();
		}
		else {
			Start();
		}
	});

	Bind(Event::Result, [=](Event &ev) {
		ev.Skip();
		last_finish_reason_type_ = std::max(last_finish_reason_type_, ev.type);

		if (waiting_for_first_result_) {
			results_ctrl_->Clear();
			waiting_for_first_result_ = false;
		}
		else {
			results_ctrl_->AppendText(wxS("\n\n"));
		}

		if (ev.type == Event::Warning) {
			auto old_style = results_ctrl_->GetDefaultStyle();
			results_ctrl_->SetDefaultStyle(fx::colors::warning);
			results_ctrl_->AppendText(_F("Test of %s finished with warnings.\n\n", ev.host));
			results_ctrl_->SetDefaultStyle(old_style);

			if (!warnings_.empty()) {
				results_ctrl_->AppendText(warnings_);
				results_ctrl_->AppendText(wxS("\n"));
			}

			results_ctrl_->AppendText(ev.reason);
		}
		else
		if (ev.type == Event::Error) {
			auto old_style = results_ctrl_->GetDefaultStyle();
			results_ctrl_->SetDefaultStyle(fx::colors::error);
			results_ctrl_->AppendText(_F("Test of %s failed.\n\n", ev.host));
			results_ctrl_->SetDefaultStyle(old_style);

			if (!errors_.empty()) {
				results_ctrl_->AppendText(errors_);
				results_ctrl_->AppendText(wxS("\n"));
			}

			results_ctrl_->AppendText(ev.reason);
		}
		else
		if (ev.type == Event::Success) {
			auto old_style = results_ctrl_->GetDefaultStyle();
			results_ctrl_->SetDefaultStyle(fx::colors::reply);
			results_ctrl_->AppendText(_F("Test of %s succeeded.\n\n", ev.host));
			results_ctrl_->SetDefaultStyle(old_style);

			results_ctrl_->AppendText(ev.reason);
		}
		else {
			results_ctrl_->SetValue(ev.reason);
		}

		if (worker_->num_or_remaining_tests() == 0) {
			wxSwitchBookTo(results_ctrl_, this);

			CallAfter([&]{DoStop();});

			static const wxString ext = _S("Look at the results pane for details.");

			if (last_finish_reason_type_ == Event::Success) {
				wxMsg::Success(_S("Test succeeded")).Ext(ext).Title(_S("Test result")).Wait();
			}
			else
			if (last_finish_reason_type_ == Event::Warning) {
				wxMsg::Warning(_S("Test finished with warnings")).Ext(ext).Title(_S("Test result")).Wait();
			}
			else
			if (last_finish_reason_type_ == Event::Error) {
				wxMsg::Error(_S("Test failed")).Ext(ext).Title(_S("Test result")).Wait();
			}
		}
	});

	Bind(Event::Log, [=](Event &ev) {
		ev.Skip();

		auto color = [&] {
			switch (ev.type) {
				case Event::Success: return fx::colors::reply;
				case Event::Warning: return fx::colors::warning;
				case Event::Error: return fx::colors::error;
				case Event::Status: return wxColour();
				case Event::Command: return fx::colors::command;
				case Event::Reply: return fx::colors::reply;
				case Event::Listing: return fx::colors::trace;
			}

			return wxColour();
		}();

		auto old_style = log_ctrl_->GetDefaultStyle();
		if (!ev.host.empty()) {
			log_ctrl_->SetDefaultStyle(wxTextAttr(*wxBLACK, wxNullColour, log_ctrl_->GetFont().MakeBold()));
			log_ctrl_->AppendText(_F("%s: ", ev.host));
			log_ctrl_->SetDefaultStyle(old_style);
		}
		if (color.IsOk()) {
			log_ctrl_->SetDefaultStyle(color);
		}
		log_ctrl_->AppendText(ev.reason);
		log_ctrl_->AppendText(wxS("\n"));
		if (color.IsOk()) {
			log_ctrl_->SetDefaultStyle(old_style);
		}

		if (ev.type == Event::Error) {
			errors_ += ev.reason;
			errors_ += wxS("\n");
		}
		else
		if (ev.type == Event::Warning) {
			warnings_ += ev.reason;
			warnings_ += wxS("\n");
		}
	});
}

void FtpTester::SetFtpOptions(const fz::ftp::server::options *ftp_opts)
{
	ftp_opts_ = ftp_opts;
}

void FtpTester::SetGetPublicIpFunc(GetPublicIpFunc func)
{
	get_public_ip_func_ = std::move(func);
}

void FtpTester::SetCreateFtpTestEnvironmentFunc(CreateFtpTestEnvironmentFunc func)
{
	create_ftp_test_environment_func_ = std::move(func);
}

void FtpTester::Start()
{
	Stop();

	auto hosts = fz::strtok(fz::to_utf8(host_ctrl_->GetValue()), ", \t");
	if (hosts.empty()) {
		host_ctrl_->SetFocusFromKbd();
		wxMsg::Error(_S("You must specify at least one host.")).Wait();
		return;
	}

	for (auto &h: hosts) {
		auto err = fx::ValidateHost(fz::to_wxString(h), false);
		if (!err.empty()) {
			wxString what = fz::to_wxString(h);
			if (what.size() > 66) {
				what = what.substr(0, 65) + wxT("\u2026"); // \u2026 is the horizontal ellipsis
			}

			host_ctrl_->SetFocusFromKbd();

			wxMsg::Error(_S("The specified host `%s' is invalid."), what).Ext(
				_S("Please enter a valid IPv4 address (e.g., 93.184.215.14), IPv6 address (e.g., 2606:2800:21f:cb07:6820:80da:af6b:8b2c), or hostname (e.g., example.com).\n\n%s"),
				err
			).Wait();

			return;
		}
	}

	if (ftp_opts_->sessions().pasv.host_override.empty()) {
		int res = wxMsg::WarningConfirm(_S("The FTP configuration does not specify an host for PASV mode.")).Ext(
			_S("This will work only if the server is directly exposed to the public internet.\n\nDo you wish to continue?")
		);

		if (res != wxID_YES) {
			return;
		}
	}

	start_stop_ctrl_->SetLabel(_S("&Stop the test"));
	wxSwitchBookTo(log_ctrl_, this);
	log_ctrl_->Clear();
	results_ctrl_->SetValue(_S("Test in progress..."));

	if (!ftp_opts_ || !get_public_ip_func_ || !create_ftp_test_environment_func_) {
		Event::Result.Queue(this, this, Event::Error, "", _S("Invalid parameters"));
		return;
	}

	auto public_ip = fx::RetrievePublicIpEx(get_public_ip_func_, fx::ipv_type::both);
	if (!public_ip) {
		Event::Log.Queue(this, this, Event::Error, "", _S("Couldn't get server's public IP(s)."));
		Event::Result.Queue(this, this, Event::Error, "", wxEmptyString);
		return;
	}
	else {
		Event::Log.Queue(this, this, Event::Status, "", _F("Server's public IP(s): %s.", *public_ip));
	}

	auto name_and_pass = create_ftp_test_environment_func_(*ftp_opts_);
	if (!name_and_pass) {
		Event::Log.Queue(this, this, Event::Error, "", _S("Couldn't create the testing environment."));
		Event::Result.Queue(this, this, Event::Error, "", wxEmptyString);
		return;
	}
	else {
		Event::Log.Queue(this, this, Event::Status, "", _F("Testing environment created. Temporary user: %s.", name_and_pass->first));
	}

	worker_.reset(new worker(
		*this,
		std::move(hosts),
		fz::strtok(fz::to_utf8(*public_ip), ", \t"),
		name_and_pass->first,
		name_and_pass->second,
		fz::to_utf8(port_ctrl_->ToString()),
		std::to_string(tls_choices_ctrl_->GetSelection()+1)
	));
}

void FtpTester::Stop()
{
	if (worker_) {
		Event::Log.Queue(this, this, Event::Status, "", _S("Test has been halted."));
		Event::Result.Queue(this, this, Event::Status, "", _S("Test has been halted."));

		DoStop();
	}
}

bool FtpTester::IsRunning()
{
	return worker_.get() != nullptr;
}

void FtpTester::DoStop()
{
	worker_.reset();
	start_stop_ctrl_->SetLabel(_S("&Start the test"));
	last_finish_reason_type_ = {};
	waiting_for_first_result_ = true;
}

void FtpTester::worker::do_test(fz::shared_context<worker*> c)
{
	auto w = c.lock();
	if (!w) {
		return;
	}

	w->num_of_remaining_tests_ = w->hosts_.size();

	for (auto &h: w->hosts_) {
		Event::Log.Queue(&w->ftp_tester_, &w->ftp_tester_, Event::Status, h, _S("Starting the test..."));

		auto qs = fz::util::proof_of_work("selftest", 20, {
			{ "PROTOCOL", w->protocol_ },
			{ "HOST", h },
			{ "PORT", w->port_ },
			{ "USER", w->username_ },
			{ "PASS", w->password_ },
			{ "version", w->http_.get_options().user_agent()}
		});

		fz::uri uri("https://ftptest.net/");
		uri.query_ = qs.to_string(false);

		enum parse_status {
			waiting,
			parsing_log,
			parsing_results,
			parsing_ended
		};

		w->http_.perform("POST", std::move(uri)).and_then([c, h, parse_status=waiting](fz::http::response::status status, fz::http::response &r) mutable {
			auto w = c.lock();
			if (!w) {
				return ECANCELED;
			}

			if (parse_status == parsing_ended) {
				r.body.clear();
				return 0;
			}

			if (r.code_type() != r.successful) {
				w->num_of_remaining_tests_ -= 1;
				Event::Result.Queue(&w->ftp_tester_, &w->ftp_tester_, Event::Error, h, _F("%s - %s", r.code_string(), r.reason));
				return ECANCELED;
			}


			if (status == fz::http::response::got_body) {
				for (auto l: fz::strtokenizer(r.body.to_view(), "\r\n", true)) {
					fz::trim(l);

					std::string_view rtrimmed_l = l;
					fz::rtrim(rtrimmed_l);

					if (parse_status == waiting) {
						if (rtrimmed_l == "<div class=\"log\">") {
							parse_status = parsing_log;
							continue;
						}
					}

					if (rtrimmed_l == "<div class=\"results\">") {
						parse_status = parsing_results;
						continue;
					}

					if (parse_status == waiting) {
						continue;
					}

					enum flag_e {
						cannot_be_result = 0b00,
						can_be_result    = 0b01,
						must_be_result   = 0b10,
					};

					static constexpr struct p_type {
						std::string_view what;
						Event::ReasonType type;
						flag_e flag;
					} p_table[] = {
						{"<p class=\"success\">", Event::Success, must_be_result   },
						{"<p class=\"warning\">", Event::Warning, can_be_result    },
						{"<p class=\"error\">",   Event::Error,   can_be_result    },
						{"<p class=\"command\">", Event::Command, cannot_be_result },
						{"<p class=\"reply\">",   Event::Reply,   cannot_be_result },
						{"<p class=\"listing\">", Event::Listing, cannot_be_result },
					};

					bool was_p = false;

					for (auto &p: p_table) {
						if (fz::starts_with(l, p.what)) {
							was_p = true;

							static constexpr std::string_view p_end = "</p>";
							if (fz::ends_with(l, p_end)) {
								l.remove_suffix(p_end.size());
							}
							else
							if (fz::ends_with(r.body.to_view(), l)) {
								// If we're here, it means we're parsing what is not actually a full line, which means that
								// the server must still send us the rest of it.
								return 0;
							}

							if (parse_status == parsing_results) {
								if (p.flag & (can_be_result | must_be_result)) {
									w->num_of_remaining_tests_ -= 1;
									Event::Result.Queue(&w->ftp_tester_, &w->ftp_tester_, p.type, h, _F("%s", fz::to_wxString(l.substr(p.what.size()))));
									parse_status = parsing_ended;
									return ECANCELED;
								}
							}
							else
							if (parse_status == parsing_log) {
								if (!(p.flag & must_be_result)) {
									Event::Log.Queue(&w->ftp_tester_, &w->ftp_tester_, p.type, h, _F("%s", fz::to_wxString(l.substr(p.what.size()))));
									break;
								}
							}
						}
					}

					if (!was_p && fz::ends_with(r.body.to_view(), l)) {
						// If we're here, it means we're parsing what is not actually a full line, which means that
						// the server must still send us the rest of it.
						return 0;
					}
				}

				r.body.clear();
			}
			else
			if (status == fz::http::response::got_end) {
				w->num_of_remaining_tests_ -= 1;
				Event::Result.Queue(&w->ftp_tester_, &w->ftp_tester_, Event::Error, h, _S("Couldn't parse the result of the test."));
			}

			return 0;
		});
	}
}
