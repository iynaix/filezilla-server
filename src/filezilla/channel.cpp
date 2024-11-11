#include "channel.hpp"

namespace fz {

namespace {

struct none_progress_notifier: public channel::progress_notifier {
	void notify_channel_socket_read_amount(const monotonic_clock &, int64_t) override{}
	void notify_channel_socket_written_amount(const monotonic_clock &, int64_t) override{}
} none_progress_notifier;

}

channel::progress_notifier &channel::progress_notifier::none = none_progress_notifier;

channel::channel(event_handler &target_handler, std::size_t max_buffer_size, std::size_t max_num_loops, bool thread_safe, progress_notifier &pn, std::size_t max_writable_amount_left_before_reading_again)
	: event_handler{target_handler.event_loop_}
	, sa_(*this, max_buffer_size, max_writable_amount_left_before_reading_again)
	, out_(*this, max_num_loops,thread_safe, "(OUT)")
	, in_(*this, max_num_loops, thread_safe, "(IN)")
	, target_handler_(target_handler)
	, pn_(pn)
{
}

channel::channel(event_handler &target_handler, std::size_t max_buffer_size, std::size_t max_num_loops, bool thread_safe, std::size_t max_writable_amount_left_before_reading_again)
	: channel(target_handler, max_buffer_size, max_num_loops, thread_safe, progress_notifier::none, max_writable_amount_left_before_reading_again)
{}

channel::~channel()
{
	remove_handler();
	sa_.remove_handler();
	in_.remove_handler();
	out_.remove_handler();

	set_socket(nullptr);
}

void channel::set_socket(socket_interface *si, bool wait_for_empty_buffer_on_eof)
{
	scoped_lock lock(mutex_);

	if (si != nullptr) {
		if (&pn_ != &progress_notifier::none) {
			in_.set_adder(&monitored_socket_read_, wait_for_empty_buffer_on_eof);
			out_.set_consumer(&monitored_socket_write_);
		}
		else {
			in_.set_adder(&sa_, wait_for_empty_buffer_on_eof);
			out_.set_consumer(&sa_);
		}
	}
	else {
		in_.set_adder(nullptr);
		out_.set_consumer(nullptr);
	}

	sa_.set_socket(si);
}

socket_interface *channel::get_socket() const
{
	scoped_lock lock(mutex_);

	return sa_.get_socket();
}

void channel::set_max_buffer_size(std::size_t max)
{
	scoped_lock lock(mutex_);

	sa_.set_max_readable_amount(max);
}

void channel::set_buffer_adder(buffer_operator::adder_interface *adder, bool wait_for_empty_buffer_on_eof)
{
	out_.set_adder(adder, wait_for_empty_buffer_on_eof);
}

void channel::set_buffer_consumer(buffer_operator::consumer_interface *consumer)
{
	in_.set_consumer(consumer);
}

void channel::clear()
{
	scoped_lock lock(mutex_);

	out_.clear();
	in_.clear();
	sa_.set_socket(nullptr);
}

void channel::operator()(const event_base &event)
{
	dispatch<
		pipe::done_event
	>(event, this,
		&channel::on_pipe_done_event
	);
}

void channel::shutdown(int err)
{
	scoped_lock lock(mutex_);

	sa_.shutdown(err);
}

void channel::dump_state(logger_interface &logger)
{
	scoped_lock lock(mutex_);

	logger.log_u(logmsg::debug_debug, L"CHANNEL state is being dumped:");
	in_.dump_state(logger);
	out_.dump_state(logger);
}

void channel::done(error_type e)
{
	scoped_lock lock(mutex_);

	if (sa_.get_socket()) {
		lock.unlock();

		in_.set_adder(nullptr);
		in_.set_consumer(nullptr);
		out_.set_adder(nullptr);
		out_.set_consumer(nullptr);

		lock.lock();
		sa_.set_socket(nullptr);

		target_handler_.send_event<channel::done_event>(*this, std::move(e));
	}
}

void channel::on_pipe_done_event(pipe &p, pipe::error_type e)
{
	auto es = error_source::socket;

	if (&p == &in_) {
		out_.clear();

		if (e.source() == pipe::error_source::consumer)
			es = error_source::buffer_consumer;
	}
	else
	if (&p == &out_) {
		in_.clear();

		if (e.source() == pipe::error_source::adder)
			es = error_source::buffer_adder;
	}

	done({e, es});
}

void channel::monitor_consumed_amount(const monotonic_clock &time_point, int64_t amount) const
{
	pn_.notify_channel_socket_written_amount(time_point, amount);
}

void channel::monitor_added_amount(const monotonic_clock &time_point, int64_t amount) const
{
	pn_.notify_channel_socket_read_amount(time_point, amount);
}

}
