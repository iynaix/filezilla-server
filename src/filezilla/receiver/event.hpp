#ifndef FZ_RECEIVER_EVENT_HPP
#define FZ_RECEIVER_EVENT_HPP

#include <libfilezilla/event_handler.hpp>
#include "../util/traits.hpp"

/*
 *  ***** !ATTENTION! *****
 *
 *  Putting this here temporarily, for lack of a better place.
 *
 *  When using the receivers, great care must be taken *NOT TO* std::move() objects into the lambda if they're also used in the async function call itself,
 *  as, by today's C++ standard, **order of evaluation of function arguments is unspecified** (as of c++17 interleaving is prohibited, but that doesn't help here).
 *
 *  Moving the receiver_handle is safe, because it's used only by async_receive(), which sits on the left side of
 *  operator>>(), which evaluates left-to-right: so first async_receive(r) takes place, then std::move(r).
 */

#include "interface.hpp"

namespace fz {

template<typename UniqueType, typename...Values>
class receiver_event: public event_base, public valued_receiver_interface<Values...>
{
public:
	using values_type = valued_receiver_interface<Values...>;
	using unique_type = UniqueType;

	static size_t type()
	{
		static size_t const v = get_unique_type_id(typeid(receiver_event*));
		return v;
	}

private:
	size_t derived_type() const override final
	{
		return type();
	}
};

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(receiver_event)

template<typename UniqueType, typename E, typename... Values>
struct extend_receiver_event;

template<typename UniqueType, typename OtherUniqueType,  typename... OtherValues, typename... Values>
struct extend_receiver_event<UniqueType, receiver_event<OtherUniqueType, OtherValues...>, Values...>
{
	using type = receiver_event<UniqueType, OtherValues..., Values...>;
};

template<typename UniqueType, typename E, typename... Values>
using extend_receiver_event_t = typename extend_receiver_event<UniqueType, E, Values...>::type;

}

#endif // FZ_RECEIVER_EVENT_HPP
