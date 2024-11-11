#ifndef FZ_RECEIVER_ASYNC_HPP
#define FZ_RECEIVER_ASYNC_HPP

#include "../mpl/prepend.hpp"
#include "../util/invoke_later.hpp"

#include "handle.hpp"
#include "detail.hpp"

namespace fz {

class async_receiver
{
public:
	enum reentrancy {
		reentrant,
		non_reentrant
	};

private:
	struct receiver_base: util::invoker_event, receiver_context::managed_object
	{
		receiver_base() = default;
		receiver_base(const receiver_base &) = delete;
		receiver_base(receiver_base &&) = delete;
		receiver_base& operator=(const receiver_base &) = delete;
		receiver_base& operator=(receiver_base &&) = delete;
	};

	template <typename F>
	struct holder_base
	{
		holder_base(shared_receiver_context &sc, F && f) noexcept
			: sc_(sc)
			, f_(std::forward<F>(f))
		{}

		shared_receiver_context sc_;
		F f_;
	};

	template <reentrancy Reentrancy, template <reentrancy, typename, typename> typename Receiver, typename F>
	struct holder: holder_base<F>
	{
		using holder_base<F>::holder_base;

		template <typename E>
		operator receiver_handle<E> () &&
		{
			using receiver = Receiver<Reentrancy, E, F>;

			constexpr bool CanCompile = receiver_detail::is_invocable_with_tuple_v<F, typename receiver::signature_type>;

			if constexpr (CanCompile) {
				if (auto &&c = this->sc_.lock()) {
					auto r = c->template create_managed<receiver>(this->sc_, std::forward<F>(this->f_));
					return {this->sc_, r, true};
				}
			}
			else {
				static_assert(CanCompile, "Callable's parameters don't match with the event.");
			}

			return {};
		}
	};

	template <reentrancy, typename E, typename F>
	struct receiver;

	template <typename E, typename F>
	struct receiver<non_reentrant, E, F> final: holder_base<F>, receiver_base, E::values_type
	{
		using holder_base<F>::holder_base;

		using signature_type = typename E::tuple_type;

		void operator()() const override
		{
			auto &self = *const_cast<receiver *>(this);
			std::apply(self.f_, self.E::values_type::v_);
			self.sc_.lock()->destroy_managed(self);
		}

		void receive_in(receiver_context &c) override
		{
			c.eh.send_persistent_event(this);
		}

		void done_in(receiver_context &) override
		{
		}

		std::size_t event_type() const override
		{
			return E::type();
		}
	};

	template <typename E, typename F>
	struct receiver<reentrant, E, F> final: holder_base<F>, receiver_base, E::values_type
	{
		using holder_base<F>::holder_base;

		using signature_type = mpl::prepend_t<typename E::tuple_type, receiver_handle<E>>;

		void operator()() const override
		{
			auto &self = *const_cast<receiver *>(this);

			self.received_ = true;

			std::apply([&self](auto &&...args) {
				self.f_(receiver_handle<E>(self.sc_, &self, true), std::forward<decltype(args)>(args)...);
			}, self.E::values_type::v_);
		}

		void receive_in(receiver_context &c) override
		{
			c.eh.send_persistent_event(this);
		}

		void done_in(receiver_context &c) override
		{
			if (received_) {
				// If we're here, it means the receiver_handle is being destroyed within the context of the event handler,
				// so it's safe to dispose of the receiver as well.
				c.destroy_managed(*this);
			}
		}

		std::size_t event_type() const override
		{
			return E::type();
		}

	private:
		bool received_{};
	};

public:
	template <reentrancy Reentrancy>
	class maker
	{
	public:
		maker(enabled_for_receiving_base *h) noexcept
			: c_(h->get_shared_receiver_context())
		{}

		maker(enabled_for_receiving_base &h) noexcept
			: c_(h.get_shared_receiver_context())
		{}

		template <typename E>
		maker(const receiver_handle<E> &h) noexcept
			: c_(h.get_shared_receiver_context())
		{}

		template <typename F>
		holder<Reentrancy, receiver, F> operator >>(F && f)
		{
			return { c_, std::forward<F>(f) };
		}

	private:
		shared_receiver_context c_;
	};
};


using async_receive = async_receiver::maker<async_receiver::non_reentrant>;
using async_reentrant_receive = async_receiver::maker<async_receiver::reentrant>;

class async_handler final: public util::invoker_handler, public enabled_for_receiving<async_handler>
{
public:
	using util::invoker_handler::invoker_handler;
	~async_handler() override
	{
		remove_handler_and_stop_receiving();
	}
};

}
#endif // FZ_RECEIVER_ASYNC_HPP
