#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/hash.hpp>

#include <wx/gauge.h>

#include "../filezilla/util/io.hpp"

#include "downloader.hpp"
#include "helpers.hpp"



static fz::logger_interface &get_logger(wxWindow *win)
{
	if (auto l = wxGetWindowLogger(win))
		return *l;

	if (auto l = wxGetWindowLogger(nullptr))
		return *l;

	return fz::get_null_logger();
}

Downloader::Downloader(wxWindow *p, fz::thread_pool &pool, fz::event_loop &loop, fz::logger_interface *logger, fz::http::client::options opts)
	: wxPanel(p)
	, logger_(logger ? *logger : get_logger(p), "wxDownloader")
	, http_(pool, loop, logger_, std::move(opts))
	, accumulator_(fz::hash_algorithm::sha512)
{
	SetName(wxT("wxDownloader"));

	//logger_.set_all(fz::logmsg::type(~0));

	wxHBox(this, 0) = {
		stop_ = new wxButton(this, wxID_STOP, _S("&Stop")),
		new wxStaticText(this, wxID_ANY, _S("Dowloading:")),
		wxSizerFlags(1) >>= gauge_ = new wxGauge(this, wxID_ANY, 0)
	};

	stop_->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
		Stop();
	});

	Bind(Event::Stopped, [this](auto &ev) {
		ev.Skip();
		stop_->Disable();
	});

	Bind(Event::Succeeded, [this](auto &ev) {
		ev.Skip();
		stop_->Disable();
	});

	stop_->Disable();
}

void Downloader::SetDownloadDir(const fz::native_string &dirname)
{
	dirname_ = dirname;
}

void Downloader::SetUrl(const std::string &url, int64_t expected_size, const std::string &sha512_hash)
{
	url_ = url;
	expected_size_ = expected_size;
	sha512_hash_ = sha512_hash;
}

bool Downloader::Start()
{
	if (downloading_.exchange(true) == true)
		return false;

	stop_->Enable();

	auto uri = fz::uri(url_);
	std::string base = fz::util::fs::unix_path(uri.path_).base();

#if defined(FZ_FINDOWS) && FZ_WINDOWS
	fz::replace_substrings(base, ':', '_');
	fz::replace_substrings(base, '\\', '_');
#endif

	filepath_ = dirname_ / fz::to_native(base);

	if (!filepath_) {
		Error(_F("Download path is invalid: %s.", filepath_));
		return true;
	}

	if (!sha512_hash_.empty()) {
		if (auto f = filepath_.open(fz::file::reading)) {
			if (f.size() == expected_size_) {
				accumulator_.reinit();
				accumulator_.update(fz::util::io::read(f).to_view());
				if (accumulator_.digest() == fz::hex_decode(sha512_hash_)) {
					Success();
					return true;
				}
			}
		}
	}

	file_ = filepath_.open(fz::file::writing, fz::file::empty);
	if (!file_) {
		Error(_F("Could not open file path '%s' for writing.", filepath_));
		return true;
	}

	http_.perform("GET", fz::uri(url_)).and_then([this, amount = std::size_t(0)](fz::http::response::status status, fz::http::response &r) mutable {
		if (r.code != 200) {
				Error(_F("HTTP %s: %s", r.code_string(), fz::removed_ctrl_chars(r.reason)));

			return ECANCELED;
		}

		if (status == fz::http::response::got_end) {
			if (accumulator_.digest() == fz::hex_decode(sha512_hash_)) {
				Success();
				return 0;
			}

			Error(_S("The downloaded file seems to be corrupted."));
			return EINVAL;
		}

		if (!downloading_)
			return ECANCELED;

		if (status == fz::http::response::got_headers) {
			auto size = fz::to_integral<int64_t>(r.headers.get("Content-Length"), -1);

			if (expected_size_ < 0)
				expected_size_ = size;

			if (size <= 0) {
				Error(_S("Got invalid size from download server"));
				return EINVAL;
			}

			if (expected_size_ != size) {
				Error(_S("The server provided a file with a size that doesn't match the expected one."));
				return EINVAL;
			}

			accumulator_.reinit();

			CallAfter([this, size] {
				gauge_->SetRange(int(size));
			});
		}
		else
		if (status == fz::http::response::got_body) {
			amount += r.body.size();

			if (expected_size_ < std::int64_t(amount)) {
				Error(_S("Too much data from server."));
				return EINVAL;
			}

			accumulator_.update(r.body.to_view());

			if (int err = 0; !fz::util::io::write(file_, r.body, &err)) {
				Error(_F("Couldn't write to file '%s': %s", filepath_, ::strerror(err)));
				return ECANCELED;
			}

			r.body.clear();

			CallAfter([this, amount] {
				gauge_->SetValue(int(amount));
			});
		}

		return 0;
	});

	return true;
}

void Downloader::Stop()
{
	if (downloading_.exchange(false) == true) {
		file_.close();
		Event::Stopped.Queue(this, this);
	}
}

bool Downloader::Running()
{
	return downloading_;
}

void Downloader::Error(const wxString &err)
{
	if (downloading_.exchange(false) == true) {
		file_.close();
		Event::Stopped.Queue(this, this, err);
	}
}

void Downloader::Success()
{
	if (downloading_.exchange(false) == true) {
		file_.close();
		Event::Succeeded.Queue(this, this, fz::to_wxString(filepath_.str()));
	}
}
