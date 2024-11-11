#ifndef FZ_BUFFER_OPERATOR_MONITORED_CONSUMER_HPP
#define FZ_BUFFER_OPERATOR_MONITORED_CONSUMER_HPP

#include "../buffer_operator/detail/monitored_base.hpp"
#include "../buffer_operator/consumer.hpp"

namespace fz::buffer_operator {

	struct consumer_monitor {
		virtual ~consumer_monitor(){}

		virtual void monitor_consumed_amount(const monotonic_clock &time_point, std::int64_t amount) const = 0;
	};

	class monitored_consumer: public detail::monitored_base<consumer_interface, &consumer_monitor::monitor_consumed_amount> {
	public:
		using monitored_base::monitored_base;

		int consume_buffer() override {
			auto buffer = monitored_.get_buffer();
			if (!buffer)
				return EINVAL;

			auto previous_amount = buffer->size();

			int error = monitored_.consume_buffer();

			if (error == 0) {
				amount_ += previous_amount - buffer->size();

				monitor();
			}

			return error;
		}
	};

}

#endif // FZ_BUFFER_OPERATOR_MONITORED_CONSUMER_HPP
