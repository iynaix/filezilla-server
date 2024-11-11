#ifndef FZ_RECEIVER_SYNC_SYNC_HPP
#define FZ_RECEIVER_SYNC_SYNC_HPP

#include "handle.hpp"
#include "detail.hpp"
#include "../util/invoke_later.hpp"

namespace fz {

class sync_receiver_maker
{
	struct receiver_base: event_handler
	{
		bool run(duration timeout = {})
		{
			if (timeout)
				add_timer(timeout, true);

			event_loop_.run();

			return has_timed_out_;
		}

		~receiver_base() override
		{
			pop_loop();
		}

	protected:
		receiver_base()
			: event_handler(push_loop())
		{}

		bool has_timed_out_{};

	private:
		using loop_stack = receiver_detail::blob_stack<sizeof(event_loop), alignof(event_loop)>;

		static event_loop &push_loop()
		{
			return *new(loop_stack::push()) event_loop{event_loop::threadless};
		}

		static void pop_loop()
		{
			reinterpret_cast<event_loop *>(loop_stack::top())->~event_loop();
			loop_stack::pop();
		}
	};

	template <typename E, typename F>
	struct receiver final: receiver_base, enabled_for_receiving<receiver<E, F>>, E
	{
		receiver(F && f) noexcept
			: f_(std::forward<F>(f))
		{
		}

		~receiver() override
		{
			this->remove_handler_and_stop_receiving();
		}

		void operator()(const fz::event_base &ev) override
		{
			if (!fz::dispatch<util::invoker_event>(ev, util::invoker_event::invoke)) {
				if (!fz::dispatch<E>(ev, f_) && ev.derived_type() == timer_event::type())
					has_timed_out_ = true;

				event_loop_.stop();
			}
		}

		void *operator new(std::size_t)
		{
			return receiver_detail::blob_stack<sizeof(receiver), alignof(receiver)>::push();
		}

		void operator delete(void *)
		{
			return receiver_detail::blob_stack<sizeof(receiver), alignof(receiver)>::pop();
		}

		void receive_in(receiver_context &) override
		{
			send_persistent_event(this);
		}

		void done_in(receiver_context &) override
		{
		}

		std::size_t event_type() const override
		{
			return E::type();
		}

	private:
		F f_;
	};

	template <typename F>
	struct holder {
		holder(F && f) noexcept
			: f_(std::forward<F>(f))
			, timeout_{}
			, has_timed_out_{not_interested_in_timeout_}
		{}

		holder(F && f, duration timeout, bool &has_timed_out) noexcept
			: f_(std::forward<F>(f))
			, timeout_(timeout)
			, has_timed_out_{has_timed_out}
		{}

		template <typename E>
		operator receiver_handle<E> () &&
		{
			constexpr bool is_callable_with_right_parameters = receiver_detail::is_invocable_with_tuple_v<F, typename E::tuple_type>;

			if constexpr (is_callable_with_right_parameters) {
				auto r = new receiver<E, F&>(f_);
				r_ = r;
				return {r, r, true};
			}
			else {
				// If we're here, it means F is not callable, or is a callable with the wrong number/type of parameters.
				// Hence, let's try to create a callable that uses F as its parameter (or list of parameters, if F is a tuple),
				// which will them move those parameters into F.
				using C = typename receiver_detail::callable_with_args<F>::type;

				constexpr bool is_callable_with_right_parameters = receiver_detail::is_invocable_with_tuple_v<C, typename E::tuple_type>;

				if constexpr (is_callable_with_right_parameters) {
					auto r = new receiver<E, C>(C(std::forward<F>(f_)));
					r_ = r;
					return {r, r, true};
				}
				else {
					static_assert(is_callable_with_right_parameters, "Callable's parameters don't match with the event.");
					return {};
				}
			}
		}

		~holder()
		{
			has_timed_out_ = r_->run(timeout_);
			delete r_;
		}

	protected:
		F f_;
		duration timeout_;
		bool &has_timed_out_;
		receiver_base *r_{};

		bool not_interested_in_timeout_;
	};

public:
	struct with_timeout
	{
		explicit with_timeout(duration timeout) noexcept
			: timeout_(timeout)
		{
		}

		template <typename F>
		auto operator >>(F && f) -> decltype(holder<F>(std::forward<F>(f)))
		{
			return { std::forward<F>(f), timeout_, has_timed_out_ };
		}

		explicit operator bool() const
		{
			return !has_timed_out_;
		}

		bool has_timed_out() const
		{
			return has_timed_out_;
		}

	private:
		duration timeout_;
		bool has_timed_out_;
	};

	template <typename F>
	auto operator >>(F && f) -> decltype(holder<F>(std::forward<F>(f)))
	{
		return { std::forward<F>(f) };
	}
};

inline auto sync_receive = sync_receiver_maker{};
using sync_timeout_receive = sync_receiver_maker::with_timeout;

}

#endif // FZ_RECEVIVER_SYNC_HPP
