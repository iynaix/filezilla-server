#include <libfilezilla/glue/wx.hpp>

#include <cinttypes>
#include <array>
#include <numeric>
#include <vector>
#include <algorithm>

#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/renderer.h>
#include <wx/statline.h>

#include "sessionlist.hpp"
#include "locale.hpp"
#include "glue.hpp"
#include "helpers.hpp"
#include "fluidcolumnlayoutmanager.hpp"

class SessionList::entries_view
{
public:
	entries_view(const sessions_map &sm)
		: sm_(sm)
	{}

	struct sentinel
	{};

	struct iterator
	{
		using value_type = const std::pair<const Session &, const Session::Entry &>;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using iterator_category = std::forward_iterator_tag;

		struct pointer
		{
			value_type *operator->() const
			{
				return &v_;
			}

		private:
			friend entries_view;

			pointer(value_type &&v)
				: v_(std::move(v))
			{}

			value_type v_;
		};

		bool operator !=(const sentinel &) const
		{
			return sit_ != sm_.end();
		}

		value_type operator*() const
		{
			const static Session::Entry idle;

			return { sit_->second, sit_->second.entries.empty() ? idle : eit_->second };
		}

		pointer operator->() const
		{
			return **this;
		}

		iterator &operator++()
		{
			if (eit_ != sit_->second.entries.end())
				++eit_;

			if (eit_ == sit_->second.entries.end()) {
				++sit_;

				if (sit_ != sm_.end())
					eit_ = sit_->second.entries.begin();
			}

			return *this;
		}

	private:
		friend entries_view;

		iterator(const sessions_map &sm, sessions_map::const_iterator sit)
			: sm_(sm)
			, sit_(sit)
		{
			if (sit_ != sm_.end())
				eit_ = sit_->second.entries.begin();
		}

		const sessions_map &sm_;
		sessions_map::const_iterator sit_;
		Session::entries_map::const_iterator eit_;
	};

	iterator cbegin() const
	{
		return { sm_, sm_.begin() };
	}

	sentinel cend() const
	{
		return {};
	}

private:
	const sessions_map &sm_;
};

SessionList::SessionList()
{}

enum ListCol {
	Date,
	SessionID,
	Protocol,
	Host,
	Username,
	Transfer,
};

bool SessionList::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size,const wxString &name)
{
	if (!wxListCtrlEx::Create(parent, winid, pos, size, wxTAB_TRAVERSAL | wxLC_VIRTUAL | wxLC_REPORT, wxDefaultValidator, name))
		return false;

	SetName(wxT("SessionList"));

	auto cl = new FluidColumnLayoutManager(this, true);

	auto add_column = [&](int idx, auto name, size_t weight, int def_size = -1) {
		auto col = InsertColumn(idx, name);
		if (def_size >= 0)
			SetColumnWidth(idx, wxDlg2Px(this, def_size));

		cl->SetColumnWeight(col, weight);
	};

	add_column(ListCol::Date,      _S("Date/Time"),  0, 64); // Hardcode default sizes that work well with the default Windows font.
	add_column(ListCol::SessionID, _S("Session ID"), 0);
	add_column(ListCol::Protocol,  _S("Protocol"),   0);
	add_column(ListCol::Host,      _S("Host"),       0);
	add_column(ListCol::Username,  _S("Username"),   0);
	add_column(ListCol::Transfer,  _S("Transfer"),   1);

	Bind(wxEVT_CONTEXT_MENU, [&](const wxContextMenuEvent &ev){
		fz::scoped_lock lock(mutex_);

		auto selected_count = GetSelectedItemCount();

		// Select the item under the mouse pointer, if none is selected
		if (selected_count <= 0) {
			long i = GetItemFromScreenPosition(ev.GetPosition());

			if (i < 0 || size_t(i) >= num_list_entries_)
				return;

			SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			selected_count = 1;
		}

		auto iterate_over_selected = [this](auto func) {
			if (long i = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); i != -1) {
				auto entries = entries_view(sessions_);
				auto entries_it = entries.cbegin();
				auto delta = i;

				do {
					std::advance(entries_it, delta);

					func(entries_it->first, entries_it->second);

					auto old_i = i;
					if (i = GetNextItem(old_i, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); i != -1) {
						delta = i-old_i;
					}
				} while (i != -1);
			}
		};

		wxMenu menu;

		enum { Kill = 1, Ban, Info };

		menu.Append(Kill, _P("Kill session", "Kill sessions", unsigned(selected_count)));
		menu.Append(Ban, _P("Ban IP Address", "Ban IP Addresses", unsigned(selected_count)));

		menu.Bind(wxEVT_MENU, [iterate_over_selected, this](auto) {
			iterate_over_selected([this] (auto &session, auto &) {
				Event event(Event::SessionNeedsToBeKilled, session.info, *this);
				ProcessWindowEvent(event);
			});
		}, Kill);

		menu.Bind(wxEVT_MENU, [iterate_over_selected, this](auto) {
			iterate_over_selected([this] (auto &session, auto &) {
				Event event(Event::IpNeedsToBeBanned, session.info, *this);
				ProcessWindowEvent(event);
			});
		}, Ban);

		if (selected_count == 1) {
			iterate_over_selected([this, &menu] (auto &session, auto &) {
				if (session.sizer_creator) {
					menu.Append(Info, _S("More info..."));
					menu.Bind(wxEVT_MENU, [info = session.info, sizer_creator = session.sizer_creator, this](auto) mutable {
							show_session_info(std::move(info), std::move(sizer_creator));
					}, Info);
				}
			});
		}

		PopupMenu(&menu);
	});

	Bind(wxEVT_LIST_CACHE_HINT, &SessionList::OnCacheHint, this);

	Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent &ev) {
		ev.Skip();

		auto row = ev.GetItem().GetId();

		if (row < 0 || size_t(row) >= num_list_entries_)
			return;

		fz::scoped_lock lock(mutex_);

		auto &session = std::next(entries_view(sessions_).cbegin(), row)->first;
		show_session_info(session.info, session.sizer_creator);
	});

	update_when_no_transfer_timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
		DelayedUpdate();
	});

	return true;
}

void SessionList::OnCacheHint(wxListEvent &event)
{
	fz::scoped_lock lock(mutex_);

	update_when_no_transfer_timer_.Stop();

	cache_from_ = event.GetCacheFrom();
	cache_to_ = event.GetCacheTo();

	if (cache_from_ < 0 || size_t(cache_from_) >= sessions_.size()) {
		cache_from_ = -1;
		cache_to_ = -1;
		return;
	}

	auto amount = cache_to_ - cache_from_ +1;
	if (amount <= 0) {
		cache_from_ = -1;
		cache_to_ = -1;
		return;
	}

	cache_.clear();
	cache_.reserve(size_t(amount));

	bool transfers_ongoing = false;

	entries_view view(sessions_);

	for (
		auto it = std::next(view.cbegin(), cache_from_);
		amount > 0 && it != view.cend();
		++it, --amount
	) {
		if (!it->second.path.empty())
			transfers_ongoing = true;

		cache_.emplace_back(it->first.info, it->second);
	}

	cache_to_ -= amount;

	cached_time_ = fz::monotonic_clock::now();
	if (transfers_ongoing)
		update_when_no_transfer_timer_.Start(1000);
}

SessionList::Session *SessionList::id2session(uint64_t id, bool emit_event)
{
	auto it = sessions_.find(id);
	if (it == sessions_.end()) {
		if (emit_event) {
			Session::Info info;
			info.id = id;

			GetEventHandler()->QueueEvent(new Event(Event::SessionNotAvailable, info, *this));
		}

		return nullptr;
	}

	return &it->second;
}

void SessionList::show_session_info(const Session::Info &info, const Session::SizerCreator &sizer_creator)
{
	wxPushDialog(this->GetParent(), wxID_ANY, _F("Info about session id %d (%s)", info.id, info.host), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER) |
	[info, sizer_creator](wxDialog *p) {
		wxVBox(p) = {
			wxGBox(p, 2, {1}) = {
				wxLabel(p, _S("Host: ")),
				wxEscapedLabel(p, info.host),

				wxLabel(p, _S("Session Id: ")),
				wxEscapedLabel(p, info.id_str),

				wxLabel(p, _S("Protocol: ")),
				wxEscapedLabel(p, info.protocol_name)
			},
			sizer_creator ? new wxStaticLine(p) : nullptr,
			sizer_creator ? sizer_creator(p, info) : nullptr,
			wxEmptySpace,
			new wxStaticLine(p),
			wxSizerFlags(0).Expand() >>= p->CreateStdDialogButtonSizer(wxOK),
		};

		p->ShowModal();
	};
}

wxString SessionList::OnGetItemText(long row, long column) const
{
	static const auto format_amount = [](auto a) {
		wxString suffix;
		auto format = wxS("%.2Lf %s");
		auto amount = (long double)a;

		if (amount < 1024) {
			suffix = _S("B");
			format = wxS("%.0Lf %s");
		}
		else
		if (amount < 1024*1024) {
			suffix = _S("KiB");
			amount /= 1024;
		}
		else
		if (amount < 1024*1024*1024) {
			suffix = _S("MiB");
			amount /= 1024*1024;
		}
		else {
			suffix = _S("GiB");
			amount /= 1024*1024*1024;
		}

		return wxString::Format(format, amount, suffix);
	};

	static const wxString second_suffix = wxS("/s");

	if (row >= 0 && row >= cache_from_ && row <= cache_to_) {
		auto &[i, e] = cache_[size_t(row - cache_from_)];

		switch (column) {
			case ListCol::SessionID:
				return i.id_str;

			case ListCol::Protocol:
				return i.protocol_name;

			case ListCol::Username:
				return i.username;

			case ListCol::Host:
				return i.host;

			case ListCol::Date:
				return i.start_time_str;

			case ListCol::Transfer: {
				if (e.path.empty())
					return _S("Idle");

				wxString ret;
				if (e.size > 0)
					ret += wxString::Format(wxT("(%s) "), format_amount(e.size));

				if (e.read > 0) {
					int64_t avg_bytes_per_sec = 0;
					int64_t inst_bytes_per_sec = 0;

					auto transfer_time = e.last_read_time - e.open_time;

					if (transfer_time) {
						auto stalled_time = cached_time_ - e.last_update;

						if (stalled_time.get_seconds() < 2) {
							stalled_time = {};
							inst_bytes_per_sec = e.read_bytes_per_sec;
						}

						avg_bytes_per_sec = e.read * 1000 / (transfer_time + stalled_time).get_milliseconds();
					}

					// \u2193 is the downwards arrow
					ret += wxString::Format(wxT("\u2193%s/s (avg %s/s, %s) "), format_amount(inst_bytes_per_sec), format_amount(avg_bytes_per_sec), format_amount(e.read));
				}

				if (e.written > 0) {
					int64_t avg_bytes_per_sec = 0;
					int64_t inst_bytes_per_sec = 0;

					auto transfer_time = e.last_written_time - e.open_time;

					if (transfer_time) {
						auto stalled_time = cached_time_ - e.last_update;

						if (stalled_time.get_seconds() < 2) {
							stalled_time = {};
							inst_bytes_per_sec = e.write_bytes_per_sec;
						}

						avg_bytes_per_sec = e.written * 1000 / (transfer_time + stalled_time).get_milliseconds();
					}


					// \u2191 is the upwards arrow
					ret += wxString::Format(wxT("\u2191%s/s (avg %s/s, %s) "), format_amount(inst_bytes_per_sec), format_amount(avg_bytes_per_sec), format_amount(e.written));
				}

				ret += e.path;

				return ret;
			}
		}
	}

	return wxEmptyString;
}

bool SessionList::Add(uint64_t session_id, fz::datetime start_time, wxString host, int address_family)
{
	fz::scoped_lock lock(mutex_);

	auto [it, success] = sessions_.emplace(session_id, Session{});
	if (!success) {
		return false;
	}

	auto &s = it->second;

	s.info.id = session_id;
	s.info.start_time = start_time;
	s.info.host = host;
	s.info.address_family = address_family;

	s.info.id_str = wxString::Format(wxS("%" PRIu64), s.info.id);
	s.info.start_time_str = fz::to_wxString(s.info.start_time);

	num_list_entries_ += 1;

	DelayedUpdate();

	return true;
}

bool SessionList::Remove(uint64_t session_id)
{
	fz::scoped_lock lock(mutex_);

	auto it = sessions_.find(session_id);
	if (it == sessions_.end()) {
		return false;
	}

	num_list_entries_ -= std::max(std::size_t(1), it->second.entries.size());
	sessions_.erase(it);

	DelayedUpdate();

	return true;
}

void SessionList::Clear()
{
	fz::scoped_lock lock(mutex_);

	num_list_entries_ = 0;
	sessions_.clear();

	DelayedUpdate();
}

std::optional<SessionList::Session::Info> SessionList::GetInfo(uint64_t session_id)
{
	fz::scoped_lock lock(mutex_);

	if (auto *s = id2session(session_id, false))
		return s->info;

	return std::nullopt;
}

void SessionList::SetUserName(uint64_t session_id, wxString username)
{
	fz::scoped_lock lock(mutex_);

	auto *s = id2session(session_id, true);
	if (!s) return;

	s->info.username = std::move(username);

	DelayedUpdate();
}

void SessionList::EntryOpen(uint64_t session_id, uint64_t entry_id, fz::duration time, wxString path, int64_t size)
{
	fz::scoped_lock lock(mutex_);

	auto *sp = id2session(session_id, true);
	if (!sp)
		return;

	auto [it, success] = sp->entries.emplace(entry_id, Session::Entry{});
	if (!success)
		return;

	auto &e = it->second;

	e.open_time = time;
	e.path = std::move(path);
	e.size = size;

	if (sp->entries.size() > 1)
		num_list_entries_ += 1;

	DelayedUpdate();
}

void SessionList::EntryClose(uint64_t session_id, uint64_t entry_id)
{
	fz::scoped_lock lock(mutex_);

	auto *sp = id2session(session_id, false);
	if (!sp)
		return;

	if (!sp->entries.erase(entry_id))
		return;

	if (sp->entries.size() > 0)
		num_list_entries_ -= 1;

	DelayedUpdate();
}

void SessionList::SetEntryWritten(uint64_t session_id, uint64_t entry_id, fz::duration time, int64_t amount, int64_t size)
{
	fz::scoped_lock lock(mutex_);

	auto *sp = id2session(session_id, true);
	if (!sp)
		return;

	auto &e = sp->entries[entry_id];

	if (e.last_written_time) {
		if (auto delta_time = time - e.last_written_time)
			e.write_bytes_per_sec = (amount - e.written)*1000/delta_time.get_milliseconds();
	}

	e.written = amount;
	e.size = size;
	e.last_written_time = time;
	e.last_update = fz::monotonic_clock::now();

	DelayedUpdate();
}

void SessionList::SetEntryRead(uint64_t session_id, uint64_t entry_id, fz::duration time, int64_t amount)
{
	fz::scoped_lock lock(mutex_);

	auto *sp = id2session(session_id, true);
	if (!sp)
		return;

	auto &e = sp->entries[entry_id];

	if (e.last_read_time) {
		if (auto delta_time = time - e.last_read_time)
			e.read_bytes_per_sec = (amount - e.read)*1000/delta_time.get_milliseconds();
	}

	e.read = amount;
	e.last_read_time = time;
	e.last_update = fz::monotonic_clock::now();

	DelayedUpdate();
}

void SessionList::SetProtocolInfo(uint64_t session_id, wxString name, SecureState state, Session::SizerCreator info_sizer_creator)
{
	fz::scoped_lock lock(mutex_);

	auto *sp = id2session(session_id, true);
	if (!sp) return;
	auto &s = *sp;

	s.info.protocol_name = name;
	s.info.secure_state = state;
	s.sizer_creator = std::move(info_sizer_creator);

	DelayedUpdate();
}

long SessionList::GetUpdatedItemCount() const
{
	fz::scoped_lock lock(mutex_);

	return static_cast<long>(num_list_entries_);
}

wxDEFINE_EVENT(SessionList::Event::SessionNeedsToBeKilled, SessionList::Event);
wxDEFINE_EVENT(SessionList::Event::IpNeedsToBeBanned, SessionList::Event);
wxDEFINE_EVENT(SessionList::Event::SessionNotAvailable, SessionList::Event);

SessionList::Event::Event(wxEventType eventType, const Session::Info &session_info, const SessionList &owner)
	: wxEvent(owner.GetId(), eventType)
	, session_info(session_info)
{}
