#ifndef FZ_CHANNEL_HPP
#define FZ_CHANNEL_HPP

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/socket.hpp>

#include "./buffer_operator/socket_adapter.hpp"
#include "./buffer_operator/monitored_adder.hpp"
#include "./buffer_operator/monitored_consumer.hpp"

#include "./pipe.hpp"

namespace fz {

	class channel final: private event_handler, private buffer_operator::adder_monitor, private buffer_operator::consumer_monitor
	{
	public:
		enum class error_source { socket, buffer_consumer, buffer_adder };
		struct error_type {
			constexpr error_type() noexcept : e_(0), s_(error_source::socket){}
			constexpr error_type(int e, error_source s) noexcept : e_{e}, s_{s}{}

			operator int() const noexcept { return e_; }

			int error() const noexcept {return e_; }
			error_source source() const noexcept { return s_; }

		private:
			int e_{};
			error_source s_{};
		};

		struct progress_notifier {
			virtual ~progress_notifier() {}

			virtual void notify_channel_socket_read_amount(const monotonic_clock &time_point, std::int64_t amount) = 0;
			virtual void notify_channel_socket_written_amount(const monotonic_clock &time_point, std::int64_t amount) = 0;

			static progress_notifier &none;
		};

		using done_event = simple_event<channel, channel&, error_type>;

		channel(event_handler &target_handler, std::size_t max_buffer_size, std::size_t max_num_loops, bool thread_safe, progress_notifier &pn = progress_notifier::none, std::size_t max_writable_amount_left_before_reading_again = std::numeric_limits<std::size_t>::max());
		channel(event_handler &target_handler, std::size_t max_buffer_size, std::size_t max_num_loops, bool thread_safe, std::size_t max_writable_amount_left_before_reading_again);
		~channel() override;

		channel(const channel &) = delete;
		channel(channel &&) = delete;

		bool operator ==(const channel &rhs) const
		{
			// Given that channels aren't copyable, a channel is equal to another IFF they're the same channel.
			return this == &rhs;
		}

		void set_socket(socket_interface *si, bool wait_for_empty_buffer_on_eof = true);
		socket_interface *get_socket() const;

		void set_max_buffer_size(std::size_t max);

		void set_buffer_adder(buffer_operator::adder_interface *adder, bool wait_for_empty_buffer_on_eof = true);
		void set_buffer_consumer(buffer_operator::consumer_interface *consumer);

		void clear();

		void shutdown(int err = 0);

		void dump_state(logger_interface &logger);

	private:
		void done(error_type e);

		void operator()(const event_base &) override;
		void on_pipe_done_event(pipe &p, pipe::error_type e);

		buffer_operator::socket_adapter sa_;
		buffer_operator::monitored_adder monitored_socket_read_{sa_, *this};
		buffer_operator::monitored_consumer monitored_socket_write_{sa_, *this};

		// It's important that the order of these following two members stays like this,
		// Because the flow of data goes from in_ to out_, implying that the code that gets
		// triggered by in_ events is going to want to write to out_.
		// Hence, out_  needs to be destroyed *after* in_. Hence, in_ needs to be declared *after* out_.
		// See Task #121 to know what effect it has doing it otherwise.
		pipe out_, in_;

		event_handler &target_handler_;
		progress_notifier &pn_;

	private:
		void monitor_consumed_amount(const monotonic_clock &time_point, int64_t amount) const override;
		void monitor_added_amount(const monotonic_clock &time_point, int64_t amount) const override;

	private:
		mutable fz::mutex mutex_{true};
	};

}

#endif // CHANNEL_HPP
