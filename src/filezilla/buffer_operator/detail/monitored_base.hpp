#ifndef FZ_BUFFER_OPERATOR_DETAIL_MONITORED_BASE_HPP
#define FZ_BUFFER_OPERATOR_DETAIL_MONITORED_BASE_HPP

#include "../../buffer_operator/detail/base.hpp"

namespace fz::buffer_operator::detail {
	template <typename T>
	struct get_class;

	template <typename RType, typename Class, typename... Args>
	struct get_class<RType (Class::*)(Args...) const> {
		using type = Class;
	};

	template <typename RType, typename Class, typename... Args>
	struct get_class<RType (Class::*)(Args...)> {
		using type = Class;
	};

	template <auto T>
	using class_t = typename get_class<decltype(T)>::type;

	template <typename Monitored, auto MonitorMethod>
	class monitored_base: public Monitored {
	public:
		monitored_base(Monitored &monitored, class_t<MonitorMethod> &monitor_obj) :
			monitored_{monitored},
			monitor_obj_{monitor_obj}
		{}

		void set_buffer(locking_buffer *b) override {
			monitored_.set_buffer(b);
		}

		util::locked_proxy<locking_buffer::proxy> get_buffer() override {
			return monitored_.get_buffer();
		}

		void set_event_handler(event_handler *eh) override {
			auto h = monitored_.get_event_handler();

			if (h && eh != h.get())
				monitor(0);

			monitored_.set_event_handler(eh);
		}

		util::locked_proxy<event_handler> get_event_handler() override {
			return monitored_.get_event_handler();
		}

		bool send_event(int error) override {
			return monitored_.send_event(error);
		}

	protected:
		void monitor(std::int64_t delta = 200) {
			if (auto now = monotonic_clock::now(); !last_monitored_time_ || (now-last_monitored_time_).get_milliseconds() >= delta) {
				last_monitored_time_ = std::move(now);
				(monitor_obj_.*MonitorMethod)(last_monitored_time_, amount_);
			}
		}

		Monitored &monitored_{};
		class_t<MonitorMethod> &monitor_obj_;
		std::int64_t amount_{};
		monotonic_clock last_monitored_time_{};
	};

}

#endif // FZ_BUFFER_OPERATOR_DETAIL_MONITORED_BASE_HPP
