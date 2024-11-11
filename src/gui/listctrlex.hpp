#ifndef WXLISTCTRLEX_HPP
#define WXLISTCTRLEX_HPP

#include <array>

#include <libfilezilla/mutex.hpp>

#include <wx/listctrl.h>
#include <wx/timer.h>

#include "eventex.hpp"

class wxListCtrlEx: public wxListCtrl
{
public:
	struct Event: wxEventEx<Event>
	{
		using wxEventEx::wxEventEx;
		Event(wxEventType eventType, long item = -1);

		inline const static Tag ItemSelecting;

		bool IsAllowed() const {
			return allowed_;
		}

		void Veto() {
			allowed_ = false;
		}

		long GetItem() const {
			return item_;
		}

	private:
		long item_{-1};
		bool allowed_{true};

		friend wxListCtrlEx;

		inline const static Tag StartRefreshingTimer;
	};


	using wxListCtrl::wxListCtrl;

	wxListCtrlEx(wxWindow *parent,
				 wxWindowID winid = wxID_ANY,
				 const wxPoint &pos = wxDefaultPosition,
				 const wxSize &size = wxDefaultSize,
				 long style = wxLC_ICON,
				 const wxValidator& validator = wxDefaultValidator,
				 const wxString &name = wxS("wxListCtrlEx"));

	bool Create(wxWindow *parent,
				wxWindowID winid = wxID_ANY,
				const wxPoint &pos = wxDefaultPosition,
				const wxSize &size = wxDefaultSize,
				long style = wxLC_ICON,
				const wxValidator& validator = wxDefaultValidator,
				const wxString &name = wxS("wxListCtrlEx"));

	wxWindow *GetMainWindow();
	const wxWindow *GetMainWindow() const;

	long GetItemFromScreenPosition(const wxPoint &point) const;

	void DelayedUpdate();

	virtual long GetUpdatedItemCount() const;

	std::vector<std::vector<wxString>> GetItems(std::initializer_list<int> columns, bool only_selected) const;

	virtual void SelectAll();

private:
	void DoUpdate();
	void OnTimer(bool start_timer);

	wxTimer timer_;
	bool refresh_required_{};
	bool timer_running_{};

	mutable fz::mutex mutex_;
};

#endif // WXLISTCTRLEX_HPP
