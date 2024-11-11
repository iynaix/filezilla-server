#ifndef FZ_BUFFER_OPERATOR_SERIALIZED_CONSUMER_HPP
#define FZ_BUFFER_OPERATOR_SERIALIZED_CONSUMER_HPP

#include <atomic>

#include "../buffer_operator/consumer.hpp"
#include "../serialization/archives/binary.hpp"

namespace fz::buffer_operator {

	class serialized_consumer: public consumer {
	private:

		enum consuming_state: std::uint8_t
		{
			consuming_enabled = 0,
			consuming_disabled = 1,
			consuming_disabled_and_expecting_event = 2
		};

		std::atomic<consuming_state> state_{};

		int consume_buffer() override {
			auto expected = consuming_disabled;
			state_.compare_exchange_strong(expected, consuming_disabled_and_expecting_event);
			if (expected != consuming_enabled)
				return EAGAIN;

			auto buffer = get_buffer();
			if (!buffer)
				return EINVAL;

			serialization::binary_input_archive loader{*buffer};

			if (!loader) {
				FZ_SERIALIZATION_DEBUG_LOG(L"CONSUME BUFFER: %d - %zu", loader.error(), buffer->size());
				return loader.error();
			}

			process_buffer_payload(loader);

			return loader.error();
		}

	protected:
		void enable_consuming(bool enable = true)
		{
			if (enable) {
				if (state_.exchange(consuming_enabled) == consuming_disabled_and_expecting_event)
					send_event(0);
			}
			else {
				auto expected = consuming_enabled;
				state_.compare_exchange_strong(expected, consuming_disabled);
			}
		}

	protected:
		virtual void process_buffer_payload(serialization::binary_input_archive &loader) = 0;
	};

}

#endif // FZ_BUFFER_OPERATOR_SERIALIZED_CONSUMER_HPP
