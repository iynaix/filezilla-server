#ifndef FTPTESTER_HPP
#define FTPTESTER_HPP

#include <wx/panel.h>

#include <libfilezilla/thread_pool.hpp>
#include <libfilezilla/event_loop.hpp>
#include <libfilezilla/logger.hpp>

#include "../filezilla/expected.hpp"
#include "../filezilla/http/client.hpp"
#include "../filezilla/ftp/server.hpp"
#include "../filezilla/logger/modularized.hpp"

#include "eventex.hpp"

class wxButton;
class IntegralEditor;
class wxChoice;
class wxTextCtrl;

class FtpTester: public wxPanel
{
public:
	using GetPublicIpFunc = std::function<fz::expected<std::string, wxString>(fz::address_type at)>;
	using CreateFtpTestEnvironmentFunc = std::function<fz::expected<std::pair<std::string, std::string>, wxString>(const fz::ftp::server::options &ftp_opts)>;

	FtpTester(wxWindow *parent, fz::thread_pool &pool, fz::event_loop &loop, fz::tls_system_trust_store *trust_store, fz::logger_interface &logger = fz::get_null_logger());

	void SetFtpOptions(const fz::ftp::server::options *ftp_opts);
	void SetGetPublicIpFunc(GetPublicIpFunc func);
	void SetCreateFtpTestEnvironmentFunc(CreateFtpTestEnvironmentFunc func);

	void Start();
	void Stop();
	bool IsRunning();


	struct Event: wxEventEx<Event>
	{
		enum ReasonType {
			Success = 1,
			Warning,
			Error,
			Status,
			Command,
			Reply,
			Listing
		};

		inline static const Tag Result;
		inline static const Tag Log;

		ReasonType type;
		std::string host;
		wxString reason;

	private:
		friend Tag;

		using wxEventEx::wxEventEx;

		Event(const Tag &tag, ReasonType type, std::string host, const wxString &reason = {})
			: wxEventEx(tag)
			, type(type)
			, host(host)
			, reason(reason)
		{}
	};

	Event::ReasonType GetLastFinishReasonType()
	{
		return last_finish_reason_type_;
	}

private:
	void DoStop();

	class worker;

	fz::thread_pool &pool_;
	fz::event_loop &loop_;
	fz::tls_system_trust_store *trust_store_{};
	fz::logger::modularized logger_;

	const fz::ftp::server::options *ftp_opts_{};
	GetPublicIpFunc get_public_ip_func_;
	CreateFtpTestEnvironmentFunc create_ftp_test_environment_func_;

	wxTextCtrl *host_ctrl_{};
	IntegralEditor *port_ctrl_{};
	wxChoice *tls_choices_ctrl_{};
	wxButton *start_stop_ctrl_{};
	wxTextCtrl *log_ctrl_{};
	wxTextCtrl *results_ctrl_{};

	wxString warnings_;
	wxString errors_;
	Event::ReasonType last_finish_reason_type_{};
	bool waiting_for_first_result_{true};

	std::unique_ptr<worker> worker_;
};

#endif // FTPTESTER_HPP
