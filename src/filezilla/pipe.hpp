#ifndef FZ_PIPE_HPP
#define FZ_PIPE_HPP

#include <variant>
#include <libfilezilla/socket.hpp>
#include <libfilezilla/event_handler.hpp>

#include "./buffer_operator/adder.hpp"
#include "./buffer_operator/consumer.hpp"

#include "./util/thread_id.hpp"

namespace fz {

	class pipe final: public event_handler
	{
		FZ_UTIL_THREAD_CHECK_INIT

		using safe_locking_consumer = util::locking_wrapper<buffer_operator::consumer_interface *, fz::mutex>;
		using unsafe_locking_consumer = util::locking_wrapper<buffer_operator::consumer_interface *, void>;
		using locking_consumer = util::locking_wrapper_interface<buffer_operator::consumer_interface *>;

		using safe_locking_adder = util::locking_wrapper<buffer_operator::adder_interface *, fz::mutex>;
		using unsafe_locking_adder = util::locking_wrapper<buffer_operator::adder_interface *, void>;
		using locking_adder = util::locking_wrapper_interface<buffer_operator::adder_interface *>;

	public:
		enum class error_source { consumer, adder };
		struct error_type {
			constexpr error_type(int e, error_source s) noexcept : e_{e == -ESHUTDOWN ? 0 : e}, s_{s}{}

			operator int() const noexcept { return e_; }

			int error() const noexcept {return e_; }
			error_source source() const noexcept { return s_; }

		private:
			int e_{};
			error_source s_{};
		};

		using done_event = simple_event<pipe, pipe&, error_type>;

		pipe(event_handler &target_handler, std::size_t max_num_loops, bool thread_safe, const char * = nullptr);
		~pipe() override;

		pipe(const pipe &) = delete;
		pipe(pipe &&) = delete;

		void set_adder(buffer_operator::adder_interface *adder, bool wait_for_empty_buffer_on_eof = true);
		void set_consumer(buffer_operator::consumer_interface *consumer);

		void dump_state(logger_interface &logger);

		void clear();

	protected:
		void done(error_type error);

	private:
		void operator()(const event_base &) override;

		void on_buffer_consumer_event(buffer_operator::consumer_interface *, int error);
		void on_buffer_adder_event(buffer_operator::adder_interface *, int error);

		bool add_to_buffer(buffer_operator::adder_interface *&adder);
		bool consume_buffer(buffer_operator::consumer_interface *&consumer);

		event_handler &target_handler_;

		std::size_t max_num_loops_{};
		bool thread_safe_{};

		std::variant<
			std::monostate,
			buffer_operator::safe_locking_buffer,
			buffer_operator::unsafe_locking_buffer
		> variant_buffer_;

		std::variant<
			std::monostate,
			safe_locking_adder,
			unsafe_locking_adder
		> variant_adder_;

		std::variant<
			std::monostate,
			safe_locking_consumer,
			unsafe_locking_consumer
		> variant_consumer_;

		buffer_operator::locking_buffer &buffer_;
		locking_adder &adder_;
		locking_consumer &consumer_;

		bool waiting_for_adder_event_{};
		bool waiting_for_consumer_event_{};

		int consumer_error_{};
		int adder_error_{};

		bool wait_for_empty_buffer_on_eof_{true};
		const char *name_{};
	};
}

#endif // FZ_PIPE_HPP
