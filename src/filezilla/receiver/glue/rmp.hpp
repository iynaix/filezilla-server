#ifndef FZ_RECEIVER_GLUE_RMP_HPP
#define FZ_RECEIVER_GLUE_RMP_HPP

#include "../../rmp/message.hpp"
#include "../../receiver/event.hpp"

namespace fz
{
	template <typename Message>
	struct make_receiver_event;

	template <typename Tag, typename... Ts>
	struct make_receiver_event<rmp::message<Tag (Ts...)>>
	{
		using type = receiver_event<Tag, Ts...>;
	};

	template <typename Message>
	using make_receiver_event_t = typename make_receiver_event<Message>::type;

}

#endif // FZ_RECEIVER_RMP_HPP
