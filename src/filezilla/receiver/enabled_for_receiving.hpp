#ifndef FZ_RECEIVER_ENABLED_FOR_RECEIVING_HPP
#define FZ_RECEIVER_ENABLED_FOR_RECEIVING_HPP

#include <libfilezilla/event_handler.hpp>

#include "context.hpp"

namespace fz {

class enabled_for_receiving_base
{
public:
	virtual ~enabled_for_receiving_base();

	const shared_receiver_context &get_shared_receiver_context() const
	{
		return sc_;
	}

	explicit operator bool() const
	{
		return bool(sc_);
	}

private:
	template <typename T>
	friend class enabled_for_receiving;

	enabled_for_receiving_base(event_handler &eh)
		: sc_(eh)
	{}

	shared_context<receiver_context> sc_;
};

template <typename H>
class enabled_for_receiving: public enabled_for_receiving_base
{
protected:
	enabled_for_receiving()
		: enabled_for_receiving_base(static_cast<event_handler &>(static_cast<H &>(*this)))
	{
		static_assert(std::is_base_of_v<enabled_for_receiving, H>, "H must be derived from enabled_for_receiving<H>");
		static_assert(std::is_base_of_v<event_handler, H>, "H must be derived from event_handler");
	}

	void remove_handler_and_stop_receiving()
	{
		static_cast<event_handler &>(static_cast<H &>(*this)).remove_handler();
		sc_.stop_sharing();
	}
};

}
#endif // ENABLED_FOR_RECEIVING_HPP
