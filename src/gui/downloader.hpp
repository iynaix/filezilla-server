#ifndef DOWNLOADER_HPP
#define DOWNLOADER_HPP

#include <libfilezilla/hash.hpp>

#include <wx/panel.h>

#include "../filezilla/http/client.hpp"
#include "../filezilla/util/io.hpp"

#include "eventex.hpp"

class wxGauge;
class wxButton;

class Downloader: public wxPanel
{
public:
	struct Event: wxEventEx<Event>
	{
		inline const static Tag Stopped;
		inline const static Tag Succeeded;

		const wxString &GetReason() const
		{
			return reason_;
		}

	private:
		friend Tag;

		using wxEventEx::wxEventEx;

		Event(const Tag &tag, const wxString &reason = {})
			: wxEventEx(tag)
			, reason_(reason)
		{}

		wxString reason_;
	};

	Downloader(wxWindow *parent, fz::thread_pool &pool, fz::event_loop &loop, fz::logger_interface *logger, fz::http::client::options opts = {});

	void SetDownloadDir(const fz::native_string &dirname);
	void SetUrl(const std::string &url, std::int64_t expected_size = -1, const std::string &sha512_hash = {});

	bool Start();
	void Stop();
	bool Running();

private:
	void Error(const wxString &err);
	void Success();

	fz::logger::modularized logger_;
	fz::http::client http_;
	fz::util::fs::absolute_native_path dirname_;
	std::string url_;
	std::int64_t expected_size_{-1};
	std::string sha512_hash_;
	fz::file file_;
	fz::util::fs::absolute_native_path filepath_;
	fz::hash_accumulator accumulator_;

	wxButton *stop_;
	wxGauge *gauge_;

	std::atomic_bool downloading_{};
};

#endif // DOWNLOADER_HPP
