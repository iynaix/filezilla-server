#ifndef FZ_BUFFER_OPERATOR_MONITORED_ADDER_HPP
#define FZ_BUFFER_OPERATOR_MONITORED_ADDER_HPP

#include "../buffer_operator/detail/monitored_base.hpp"
#include "../buffer_operator/adder.hpp"

namespace fz::buffer_operator {

	struct adder_monitor {
		virtual ~adder_monitor(){}

		virtual void monitor_added_amount(const monotonic_clock &time_point, std::int64_t amount) const = 0;
	};

	class monitored_adder: public detail::monitored_base<adder_interface, &adder_monitor::monitor_added_amount> {
	public:
		using monitored_base::monitored_base;

		int add_to_buffer() override {
			auto buffer = monitored_.get_buffer();
			if (!buffer)
				return EINVAL;

			auto previous_amount = buffer->size();

			int error = monitored_.add_to_buffer();

			if (error == 0) {
				amount_ += buffer->size() - previous_amount;

				monitor();
			}

			return error;
		}
	};

}

#endif // FZ_BUFFER_OPERATOR_MONITORED_ADDER_HPP
