#ifndef FZ_BUFFER_OPERATOR_SOCKET_ADAPTER_HPP
#define FZ_BUFFER_OPERATOR_SOCKET_ADAPTER_HPP

#include <cassert>
#include <libfilezilla/socket.hpp>

#include "../buffer_operator/adder.hpp"
#include "../buffer_operator/consumer.hpp"
#include "../logger/stdio.hpp"

namespace fz::buffer_operator {

	class socket_adapter final: public event_handler, public adder, public consumer {
	public:
		socket_adapter(event_handler &target_handler, std::size_t max_readable_amount, std::size_t max_writable_amount_left_before_reading_again = std::numeric_limits<std::size_t>::max());
		~socket_adapter() override;

		void set_socket(socket_interface *si);
		socket_interface *get_socket() const;

		void set_max_readable_amount(std::size_t max);

		void shutdown(int err = 0);

	private:
		int consume_buffer() override;
		int add_to_buffer() override;

	private:
		void operator()(const event_base &ev) override;
		void on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error);
		void on_shutdown_event();

		socket_interface *si_{};
		std::size_t max_readable_amount_;
		std::size_t max_writable_amount_left_before_reading_again_;
		bool waiting_for_read_event_{};
		bool waiting_for_write_event_{};
		bool adder_waiting_for_consumable_amount_to_be_low_enough_{};
		bool shutting_down_{};
		int shutdown_err_{};
	};

}

#endif // FZ_BUFFER_OPERATOR_SOCKET_ADAPTER_HPP
