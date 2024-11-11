#ifndef BUFFER_OPERATORS_HPP
#define BUFFER_OPERATORS_HPP

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/event_handler.hpp>

#include "../../util/locking_wrapper.hpp"

namespace fz::buffer_operator
{

	using safe_locking_buffer = util::locking_wrapper<buffer, fz::mutex>;
	using unsafe_locking_buffer = util::locking_wrapper<buffer, void>;
	using locking_buffer = util::locking_wrapper_interface<buffer>;

}

namespace fz::buffer_operator::detail
{
	class base_interface {
	public:
		virtual ~base_interface(){}

		virtual void set_buffer(locking_buffer *b) = 0;
		virtual util::locked_proxy<locking_buffer::proxy> get_buffer() = 0;
		virtual void set_event_handler(event_handler *eh)  = 0;
		virtual util::locked_proxy<event_handler> get_event_handler() = 0;
		virtual bool send_event(int error) = 0;
	};

	template <typename Interface>
	class base: public Interface {
	public:
		using event = simple_event<base, Interface* /*source*/, int /*error*/>;

		~base() override {
			base::set_event_handler(nullptr);
		}

		void set_buffer(locking_buffer *b) override {
			fz::scoped_lock lock(buffer_mutex_);
			buffer_ = b;
		}

		util::locked_proxy<locking_buffer::proxy> get_buffer() override {
			buffer_mutex_.lock();

			if (buffer_)
				return { buffer_->lock(), &buffer_mutex_ };

			buffer_mutex_.unlock();

			return {};
		}

		void copy_buffer_to(Interface &receiver) {
			fz::scoped_lock lock(buffer_mutex_);
			receiver.set_buffer(buffer_);
		}

		void set_event_handler(event_handler *new_handler) override {
			fz::scoped_lock lock(eh_mutex_);

			if (event_handler_ == new_handler)
				return;

			if (event_handler_) {
				auto op_event_filter = [this, new_handler](fz::event_handler *& h, fz::event_base & ev) -> bool {
					if (h == event_handler_ && same_type<event>(ev)) {
						if (new_handler)
							h = new_handler;
						else
							return std::get<0>(static_cast<event&>(ev).v_) == this;
					}

					return false;
				};

				event_handler_->event_loop_.filter_events(std::cref(op_event_filter));
			}

			event_handler_ = new_handler;
		}

		util::locked_proxy<event_handler> get_event_handler() override {
			eh_mutex_.lock();

			return { event_handler_, &eh_mutex_ };
		}

		bool send_event(int error) override {
			fz::scoped_lock lock(eh_mutex_);

			if (event_handler_) {
				event_handler_->send_event<event>(this, error);
				return true;
			}

			return false;
		}

	private:
		locking_buffer *buffer_{};
		event_handler *event_handler_{};
		fz::mutex eh_mutex_;
		fz::mutex buffer_mutex_;
	};

	template <typename Interface>
	class delegate_base: public Interface
	{
	public:
		delegate_base(Interface &delegate)
			: delegate_(delegate)
		{}

		void set_buffer(locking_buffer *b) override
		{
			delegate_.set_buffer(b);
		}

		util::locked_proxy<locking_buffer::proxy> get_buffer() override
		{
			return delegate_.get_buffer();
		}

		void set_event_handler(event_handler *eh) override
		{
			delegate_.set_event_handler(eh);
		}

		util::locked_proxy<event_handler> get_event_handler() override
		{
			return delegate_.get_event_handler();
		}

		bool send_event(int error) override
		{
			return delegate_.send_event(error);
		}

	protected:
		Interface &delegate_;
	};

}

#endif // BUFFER_OPERATORS_HPP
