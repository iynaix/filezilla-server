#ifndef WXFZFTPSESSIONLIST_HPP
#define WXFZFTPSESSIONLIST_HPP

#include <libfilezilla/time.hpp>
#include <libfilezilla/mutex.hpp>

#include <map>
#include <memory>
#include <optional>
#include <functional>

#include <wx/panel.h>
#include <wx/timer.h>

#include "listctrlex.hpp"

class SessionList: public wxListCtrlEx
{
public:
	enum SecureState { Insecure, QuasiSecure, Secure };

	struct Session {
		struct Info {
			uint64_t id{};
			wxString id_str{};
			wxString username{};
			fz::datetime start_time{};
			wxString start_time_str{};
			wxString host{};
			int address_family{};
			wxString protocol_name{};
			SecureState secure_state{};

			Info(){}
		} info{};

		struct Entry {
			wxString path{};
			int64_t size{};
			int64_t written{};
			int64_t read{};
			fz::duration open_time{};
			fz::duration last_written_time{};
			fz::duration last_read_time{};

			int64_t write_bytes_per_sec{};
			int64_t read_bytes_per_sec{};

			fz::monotonic_clock last_update;
		};

		using entries_map = std::map<std::uint64_t, Entry>;
		entries_map entries;

		using SizerCreator = std::function<wxSizer *(wxWindow *, const Info &)>;
		SizerCreator sizer_creator;

		Session(){}
	};

private:
	using sessions_map = std::map<std::uint64_t, Session>;
	sessions_map sessions_{};

public:
	SessionList();

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				const wxString& name = wxS("SessionList"));

	bool Add(uint64_t session_id, fz::datetime start_time, wxString host, int address_family);
	bool Remove(uint64_t session_id);
	void Clear();
	std::optional<SessionList::Session::Info> GetInfo(uint64_t session_id);

	void SetUserName(uint64_t session_id, wxString username);
	void SetProtocolInfo(uint64_t session_id, wxString name, SecureState state, Session::SizerCreator info_sizer_creator);

	void EntryOpen(uint64_t session_id, uint64_t entry_id, fz::duration time, wxString path, int64_t size);
	void EntryClose(uint64_t session_id, uint64_t entry_id);
	void SetEntryWritten(uint64_t session_id, uint64_t entry_id, fz::duration time, int64_t amount, int64_t size);
	void SetEntryRead(uint64_t session_id, uint64_t entry_id, fz::duration time, int64_t amount);

	struct Event: wxEvent {
		virtual Event *Clone() const override {
			return new Event(*this);
		}

		const static wxEventTypeTag<Event> SessionNeedsToBeKilled;
		const static wxEventTypeTag<Event> IpNeedsToBeBanned;
		const static wxEventTypeTag<Event> SessionNotAvailable;

		Session::Info session_info;

	private:
		friend SessionList;

		Event(wxEventType eventType, const Session::Info &session_info, const SessionList &owner);
	};

private:
	long GetUpdatedItemCount() const override;
	wxString OnGetItemText(long item, long column) const override;
	void OnCacheHint(wxListEvent &event);

	Session *id2session(uint64_t id, bool emit_event);
	void show_session_info(const Session::Info &info, const Session::SizerCreator &sizer_creator);

	class entries_view;
	std::vector<std::pair<Session::Info, Session::Entry>> cache_;
	long cache_from_{};
	long cache_to_{};
	fz::monotonic_clock cached_time_{};
	wxTimer update_when_no_transfer_timer_{};

	std::size_t num_list_entries_{};

	mutable fz::mutex mutex_{true};
};

#endif // WXFZFTPSESSIONLIST_HPP
