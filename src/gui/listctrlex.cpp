#include <libfilezilla/glue/wx.hpp>

#include "listctrlex.hpp"
#include "locale.hpp"

wxListCtrlEx::wxListCtrlEx(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxValidator &validator, const wxString &name)
{
	Create(parent, winid, pos, size, style, validator, name);
}

bool wxListCtrlEx::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxValidator &validator, const wxString &name)
{
	if (!wxListCtrl::Create(parent, winid, pos, size, style, validator, name))
		return false;

	#if defined(__WXMSW__) && wxCHECK_VERSION(3, 2, 1)
		//This gets rid of vertical lines between columns
		EnableSystemTheme(false);
	#endif

	timer_.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
		OnTimer(false);
	});

	Bind(Event::StartRefreshingTimer, [&](Event &) {
		OnTimer(true);
	});


	auto on_left_click = [this](wxMouseEvent &ev) {
		int flags = 0;
		auto row = HitTest({ev.GetX(), ev.GetY()}, flags);

		if (row != -1 && Event::ItemSelecting.Process(this, this, row).IsAllowed())
			ev.Skip();
	};

	Bind(wxEVT_LEFT_DOWN, on_left_click);
	Bind(wxEVT_LEFT_DCLICK, on_left_click);
	Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &ev) {
		ev.Skip();

		if (GetWindowStyle() & wxLC_SINGLE_SEL)
			return;

		if (ev.GetKeyCode() == 'A' && ev.ControlDown())
			CallAfter([this]{SelectAll();});
	});

	return true;
}

wxWindow *wxListCtrlEx::GetMainWindow()
{
	#ifdef __WXMSW__
		return this;
	#else
		return reinterpret_cast<wxWindow*>(m_mainWin);
	#endif
}

const wxWindow *wxListCtrlEx::GetMainWindow() const
{
	#ifdef __WXMSW__
		return this;
	#else
		return reinterpret_cast<const wxWindow*>(m_mainWin);
	#endif
}

long wxListCtrlEx::GetItemFromScreenPosition(const wxPoint &point) const
{
	int flags = 0;
	return HitTest(GetMainWindow()->ScreenToClient(point), flags);
}

void wxListCtrlEx::DoUpdate()
{
	auto old_count = GetItemCount();
	auto new_count = GetUpdatedItemCount();

	if (old_count <= new_count) {
		SetItemCount(new_count);
		Refresh();

		bool must_scroll = (new_count > 0) && (old_count-GetCountPerPage() <= GetTopItem());

		if (must_scroll)
			EnsureVisible(new_count-1);
	}
	else {
		EnsureVisible(new_count);
		SetItemCount(new_count);
		Refresh();
	}
}

void wxListCtrlEx::DelayedUpdate()
{
	fz::scoped_lock lock(mutex_);

	refresh_required_ = true;

	if (!timer_running_) {
		Event::StartRefreshingTimer.Queue(this, this);
		timer_running_ = true;
	}
}

long wxListCtrlEx::GetUpdatedItemCount() const
{
	return long(GetItemCount());
}

std::vector<std::vector<wxString> > wxListCtrlEx::GetItems(std::initializer_list<int> columns, bool only_selected) const
{
	fz::scoped_lock lock(mutex_);

	std::vector<std::vector<wxString>> lines;

	auto get_col_name = [&](int i) {
		wxListItem li;
		li.SetMask(wxLIST_MASK_TEXT);
		GetColumn(i, li);
		return li.GetText();
	};

	auto append_line = [&](const auto &get_name) {
		std::vector<wxString> line;
		for (auto c: columns) {
			if (c >= 0) {
				line.push_back(get_name(c));
			}
		}
		lines.push_back(std::move(line));
	};

	append_line(get_col_name);

	if (only_selected > 0) {
		for (long item = -1;;) {
			item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if (item == -1)
				break;

			append_line([&](int c){ return OnGetItemText(item, c); });
		}
	}
	else {
		for (long item = 0, end_item = GetItemCount(); item != end_item; ++item) {
			append_line([&](int c){ return OnGetItemText(item, c); });
		}
	}

	return lines;
}

void wxListCtrlEx::SelectAll()
{
	Freeze();

	for (long i = 0, end_i = GetItemCount(); i != end_i; ++i) {
		SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}

	Thaw();
}

void wxListCtrlEx::OnTimer(bool start_timer)
{
	fz::scoped_lock lock(mutex_);

	if (refresh_required_) {
		refresh_required_ = false;

		lock.unlock();

		DoUpdate();

		if (start_timer)
			timer_.Start(100);
	}
	else {
		timer_.Stop();
		timer_running_ = false;
	}
}

wxListCtrlEx::Event::Event(wxEventType eventType, long item)
	: wxEventEx(eventType)
	, item_(item)
{}
