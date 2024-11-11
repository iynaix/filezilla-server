#ifndef EVENTEX_HPP
#define EVENTEX_HPP

#include <wx/event.h>
#include <wx/window.h>

template <typename Derived>
struct wxEventTypeTagEx: wxEventTypeTag<Derived>
{
	wxEventTypeTagEx(): wxEventTypeTag<Derived>(wxNewEventType()) {}

	template <typename... Args>
	auto Process(wxWindow *source, wxWindow *dest, Args &&... args) const -> decltype(Derived(*this, std::forward<Args>(args)...))
	{
		Derived newev(*this, std::forward<Args>(args)...);
		newev.SetEventObject(source);
		if (!dest->ProcessWindowEvent(newev))
			newev.Skip();

		return newev;
	}

	template <typename... Args>
	auto Queue(wxWindow *source, wxWindow *dest, Args &&... args) const -> decltype(Derived(*this, std::forward<Args>(args)...), void())
	{
		auto newev = new Derived(*this, std::forward<Args>(args)...);
		newev->SetEventObject(source);
		dest->GetEventHandler()->QueueEvent(newev);
	}


	template <typename... Args>
	Derived operator()(Args &&... args) const
	{
		return {*this, std::forward<Args>(args)...};
	}
};

template <typename Derived>
class wxEventEx: public wxEvent
{
public:
	wxEventEx(const wxEventTypeTag<Derived> &tag)
		:wxEvent(wxID_ANY, tag)
	{
		m_propagationLevel = wxEVENT_PROPAGATE_MAX;
	}

	using Tag = wxEventTypeTagEx<Derived>;

	explicit operator bool() const
	{
		return !GetSkipped();
	}

private:
	wxEventEx *Clone() const override
	{
		return new Derived(*static_cast<const Derived*>(this));
	}
};

namespace wxPrivate {

template <typename Derived>
struct EventClassOf<wxEventTypeTagEx<Derived>>
{
	using type = Derived;
};

}
#endif // EVENTEX_HPP
