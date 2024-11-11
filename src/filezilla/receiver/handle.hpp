#ifndef FZ_RECEIVER_HANDLE_HPP
#define FZ_RECEIVER_HANDLE_HPP

#include <libfilezilla/event_handler.hpp>
#include <cassert>

#include "event.hpp"
#include "enabled_for_receiving.hpp"

namespace fz {

/*
 * receiver_handle<E> only accepts E's which are specializations of receiver_event<>.
 * A receiver_event<> is like a simple_event<>, and has a v_ tuple member which can be used by fz::dispatch,
 * but the v_ member isn't constructed, even if the event itself is, until the event is sent.
 *
 * The event E can only be constructed by receiver_handle<E>, and it can thus only be sent by receiver_handle<E>,
 * via its call operator.
 */

class receiver_handle_base
{
public:
	~receiver_handle_base();

	receiver_handle_base(const receiver_handle_base &) = delete;
	receiver_handle_base &operator=(const receiver_handle_base &) = delete;

	receiver_handle_base(receiver_handle_base &&rhs) noexcept
		: c_(std::move(rhs.c_))
		, r_(rhs.r_)
		, managed_(rhs.managed_)
	{
		rhs.r_ = nullptr;
	}

	receiver_handle_base &operator=(receiver_handle_base &&rhs) noexcept
	{
		receiver_handle_base moved(std::move(rhs));

		using std::swap;

		swap(*this, moved);

		return *this;
	}

	explicit operator bool() const
	{
		return r_ && c_;
	}

	const shared_receiver_context &get_shared_receiver_context() const
	{
		return c_;
	}

	// One shot invoker. Once you activate this, subsequent calls will just be no-ops. Be advised.
	template <typename E, typename... Args>
	auto execute(Args &&... args) const -> decltype(typename E::tuple_type(std::forward<Args>(args)...), void())
	{
		if (r_) {
			if (auto &&c = c_.lock()) {
				if (r_->event_type() == E::type()) {
					static_cast<typename E::values_type*>(r_)->receive_in(*c, std::forward<Args>(args)...);
				}
				else {
					assert("Attempt to invoke receiver_handle_base::execute with the wrong event type" && false);
				}
			}

			r_ = nullptr;
		}
	}

	friend void swap(receiver_handle_base &lhs, receiver_handle_base &rhs) noexcept
	{
		using std::swap;

		swap(lhs.c_, rhs.c_);
		swap(lhs.r_, rhs.r_);
		swap(lhs.managed_, rhs.managed_);
	}

protected:
	mutable shared_receiver_context c_;
	mutable receiver_interface *r_;
	bool managed_;

	receiver_handle_base(const shared_receiver_context &c, receiver_interface *r, bool managed) noexcept
		: c_(c)
		, r_(r)
		, managed_{managed}
	{}
};

template <typename E = receiver_event<void>>
class receiver_handle;

template <typename UniqueType, typename... Values>
class receiver_handle<receiver_event<UniqueType, Values...>> final: public receiver_handle_base
{
	using event_type = receiver_event<UniqueType, Values...>;
	using values_type = typename event_type::values_type;
	using tuple_type = typename event_type::tuple_type;

	class unmanaged_receiver final: public event_type
	{
	private:
		void receive_in(receiver_context &rc) override
		{
			rc.eh.send_event(this);
		}

		void done_in(receiver_context &) override
		{
			assert("done_in() called for unmanaged_receiver, but it should have never happened" && false);
		}

		std::size_t event_type() const override
		{
			return receiver_handle::event_type::type();
		}
	};

public:
	using receiver_handle_base::receiver_handle_base;

	receiver_handle()
		: receiver_handle_base({}, {}, {})
	{}

	receiver_handle(const shared_receiver_context &c, values_type *r, bool managed)
		: receiver_handle_base(c, r, managed)
	{
	}

	receiver_handle(enabled_for_receiving_base *h, values_type *r, bool managed)
		: receiver_handle_base(h->get_shared_receiver_context(), r, managed)
	{
	}

	receiver_handle(enabled_for_receiving_base &h, values_type *r, bool managed)
		: receiver_handle_base(h.get_shared_receiver_context(), r, managed)
	{
	}

	receiver_handle(enabled_for_receiving_base *h)
		: receiver_handle_base(h->get_shared_receiver_context(), new unmanaged_receiver, false)
	{
	}

	receiver_handle(enabled_for_receiving_base &h)
		: receiver_handle_base(h.get_shared_receiver_context(), new unmanaged_receiver, false)
	{
	}

	// One shot invoker. Once you activate this, subsequent calls will just be no-ops. Be advised.
	template <typename... Args>
	auto operator()(Args &&... args) const -> decltype(tuple_type(std::forward<Args>(args)...), void())
	{
		if (r_) {
			if (auto &&c = c_.lock()) {
				static_cast<values_type*>(r_)->receive_in(*c, std::forward<Args>(args)...);
			}

			r_ = nullptr;
		}
	}
};

}
#endif // FZ_RECEIVER_HANDLE_HPP
