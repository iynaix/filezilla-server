#ifndef FZ_RMP_GLUE_RECEIVER_HPP
#define FZ_RMP_GLUE_RECEIVER_HPP

#include "../message.hpp"
#include "../../receiver/event.hpp"

namespace fz::rmp
{

	template <typename E>
	struct make_message;

	template <typename Unique, typename... Args>
	struct make_message<fz::receiver_event<Unique, Args...>>
	{
		using type = message<Unique (Args...)>;
	};

	template <typename E>
	using make_message_t = typename make_message<E>::type;

}

#endif // FZ_RMP_GLUE_RECEIVER_HPP
