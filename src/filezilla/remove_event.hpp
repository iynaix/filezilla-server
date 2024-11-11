#ifndef FZ_REMOVE_EVENT_HPP
#define FZ_REMOVE_EVENT_HPP

#include <libfilezilla/event_handler.hpp>

namespace fz {

template <typename... Es, typename Source>
std::size_t remove_events(fz::event_handler *eh, const Source &s)
{
	if (!eh)
		return 0;

	std::size_t removed{};

	auto events_filter = [&](fz::event_base & ev) -> bool {
		if constexpr (std::is_pointer_v<Source>) {
			if (((ev.derived_type() == Es::type() && std::get<0>(static_cast<Es const&>(ev).v_) == s) || ...)) {
				++removed;
				return true;
			}
		}
		else {
			if (((ev.derived_type() == Es::type() && &std::get<0>(static_cast<Es const&>(ev).v_) == &s) || ...)) {
				++removed;
				return true;
			}
		}

		return false;
	};

	eh->filter_events(std::cref(events_filter));

	return removed;
}

}
#endif // FZ_REMOVE_EVENT_HPP
