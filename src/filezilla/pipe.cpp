#include <libfilezilla/logger.hpp>

#include "./pipe.hpp"
#include "util/demangle.hpp"
#include "util/thread_id.hpp"

namespace fz {

	pipe::pipe(event_handler &target_handler, std::size_t max_num_loops, bool thread_safe, const char *name)
		: event_handler(target_handler.event_loop_)
		, target_handler_(target_handler)
		, max_num_loops_(max_num_loops)
		, thread_safe_(thread_safe)
		, variant_buffer_()
		, variant_adder_()
		, variant_consumer_()
		, buffer_(thread_safe
			? (buffer_operator::locking_buffer&)variant_buffer_.emplace<buffer_operator::safe_locking_buffer>()
			: (buffer_operator::locking_buffer&)variant_buffer_.emplace<buffer_operator::unsafe_locking_buffer>()
		)
		, adder_(thread_safe
			? (locking_adder&)variant_adder_.emplace<safe_locking_adder>()
			: (locking_adder&)variant_adder_.emplace<unsafe_locking_adder>()
		)
		, consumer_(thread_safe
			? (locking_consumer&)variant_consumer_.emplace<safe_locking_consumer>()
			: (locking_consumer&)variant_consumer_.emplace<unsafe_locking_consumer>()
		)
		, name_(name)
	{
	}

	pipe::~pipe()
	{
		remove_handler();
		clear();
	}

	void pipe::set_adder(buffer_operator::adder_interface *adder, bool wait_for_empty_buffer_on_eof)
	{
		auto ap = adder_.lock();
		auto &a = *ap;

		if (a != adder) {
			if (a) {
				waiting_for_adder_event_ = false;
				a->set_event_handler(nullptr);
			}

			a = adder;
			adder_error_ = 0;

			if (a) {
				a->set_buffer(&buffer_);
				a->set_event_handler(this);
			}
		}

		wait_for_empty_buffer_on_eof_ = wait_for_empty_buffer_on_eof;
	}

	void pipe::set_consumer(buffer_operator::consumer_interface *consumer)
	{
		auto cp = consumer_.lock();
		auto &c = *cp;

		if (c != consumer) {
			if (c) {
				waiting_for_consumer_event_ = false;
				c->set_event_handler(nullptr);
			}

			c = consumer;
			consumer_error_ = 0;

			if (c) {
				c->set_buffer(&buffer_);
				c->set_event_handler(this);

				waiting_for_consumer_event_ = c->send_event(0);
			}
		}
	}

	void pipe::dump_state(logger_interface &logger)
	{
		logger.log_u(logmsg::debug_debug,
			L"PIPE(%s) {\n"
			L"    max_num_loops_           = %d\n"
			L"    thread_safe_             = %d\n"
			L"    variant_buffer_          = <%d>\n"
			L"    variant_adder_           = <%d>\n"
			L"    variant_consumer_        = <%d>\n"
			L"    buffer_                  = %p\n"
			L"    adder_                   = %p\n"
			L"    consumer_                = %p\n"
			L"    waiting_for_adder_event_ = %d\n"
			L"    consumer_error_          = %d\n"
			L"    adder_error_             = %d\n"
			L"}",
			name_,
			max_num_loops_,
			thread_safe_,
			variant_buffer_.index(),
			variant_adder_.index(),
			variant_consumer_.index(),
			&*buffer_.lock(),
			*adder_.lock().get(),
			*consumer_.lock().get(),
			waiting_for_adder_event_,
			consumer_error_,
			adder_error_
		);
	}

	void pipe::clear()
	{
		set_consumer(nullptr);
		set_adder(nullptr);

		buffer_.lock()->resize(0);
	}

	void pipe::done(error_type error) {
		FZ_UTIL_THREAD_CHECK

		clear();

		target_handler_.send_event<done_event>(*this, error);
	}

	void pipe::on_buffer_adder_event(buffer_operator::adder_interface *, int error){
		FZ_UTIL_THREAD_CHECK

		if (error) {
			return done({error, error_source::consumer});
		}

		bool is_done = false;
		{
			auto adder_p = adder_.lock();
			auto &adder = *adder_p;

			waiting_for_adder_event_ = false;

			if (!add_to_buffer(adder)) {
				is_done = true;
			}
		}

		if (is_done) {
			return done({adder_error_, error_source::adder});
		}

		if (auto consumer_p = consumer_.lock()) {
			if (!waiting_for_consumer_event_ && !buffer_.lock()->empty()) {
				auto &consumer = *consumer_p;

				if (consumer)
					waiting_for_consumer_event_ = consumer->send_event(0);
			}
		}
	}

	void pipe::on_buffer_consumer_event(buffer_operator::consumer_interface *, int error){
		FZ_UTIL_THREAD_CHECK

		if (error) {
			return done({error, error_source::consumer});
		}

		bool is_done = false;
		{
			auto consumer_p = consumer_.lock();
			auto &consumer = *consumer_p;

			waiting_for_consumer_event_ = false;

			if (!consume_buffer(consumer)) {
				is_done = true;
			}
			else {
				// consumer might have been set to nullptr in the context of consume_buffer() execution
				if (!consumer)
					return;

				if (consumer_error_ != ENODATA && !buffer_.lock()->empty()) {
					if (!waiting_for_consumer_event_)
						waiting_for_consumer_event_ = consumer->send_event(0);

					return;
				}
			}
		}

		if (is_done) {
			return done({consumer_error_, error_source::consumer});
		}

		{
			auto adder_p = adder_.lock();
			auto adder = *adder_p;

			if (!waiting_for_adder_event_ && adder)
				waiting_for_adder_event_ = adder->send_event(0);
		}
	}


	bool pipe::add_to_buffer(buffer_operator::adder_interface *&adder) {
		FZ_UTIL_THREAD_CHECK

		// Error from a previous run, we want to report now.
		if (adder_error_)
			return false;

		for (std::size_t loop = 0; adder && loop < max_num_loops_; ++loop) {
			adder_error_ = adder->add_to_buffer();

			if (!*consumer_.lock()) {
				switch (adder_error_) {
					case EAGAIN: {
						adder_error_ = 0;
						waiting_for_adder_event_ = true;
						return true;
					}

					case ENODATA: {
						adder_error_ = 0;
						return true;
					}
				}

				adder_error_ = EINVAL;
				return false;
			}

			switch (adder_error_) {
				case 0: break;

				case EAGAIN: {
					adder_error_ = 0;
					waiting_for_adder_event_ = true;
					return true;
				}

				case ENODATA: {
					// ENODATA is used to signal EOF, but outside of here EOF is not an error.
					adder_error_ = 0;

					if (!wait_for_empty_buffer_on_eof_ || buffer_.lock()->empty())
						// All data was flushed, we're done.
						return false;

					if (consumer_error_ == ENODATA)
						// Even though the buffer still contains data, it's not enough to satisfy the consumer needs. Thus, we're done.
						return false;

					// We've still got data to flush, keep going.
					return true;
				}

				case ENOBUFS: {
					// Buffer is full, we can't keep adding to it.
					if (loop == 0 && consumer_error_ == ENODATA)
						// And if the consumer couldn't do much with the data it already had, there's nothing we can do about it.
						return false;

					adder_error_ = 0;
					return true;
				}

				default: {
					return loop > 0;
				}
			}
		}

		return true;
	}

	bool pipe::consume_buffer(buffer_operator::consumer_interface *&consumer) {
		FZ_UTIL_THREAD_CHECK

		for (std::size_t loop = 0; consumer && !buffer_.lock()->empty() && loop < max_num_loops_; ++loop) {
			consumer_error_ = consumer->consume_buffer();

			if (consumer_error_) {
				// If EAGAIN is returned, it means the consumer was not ready to consume the data and the pipe needs to try again later, with the **same** data.
				// The consumer will send a consume_event whenever it's ready to handle the data.
				if (consumer_error_ == EAGAIN) {
					waiting_for_consumer_event_ = true;
					break;
				}

				// If ENODATA is returned, it means the consumer didn't find the data it wanted and the pipe needs to try again later, with **different** data.
				// **different** might also just mean **more** data.
				if (consumer_error_ == ENODATA)
					break;

				// If ECANCELED is returned, it means the consumer doesn't want to continue consuming and the pipe is done, but otherwise no error has to be reported upstream.
				if (consumer_error_ == ECANCELED)
					consumer_error_ = 0;

				return false;
			}
		}

		return true;
	}

	void pipe::operator()(const event_base &event)
	{
		FZ_UTIL_THREAD_CHECK

		dispatch<
			buffer_operator::adder::event,
			buffer_operator::consumer::event
		>(event, this,
			&pipe::on_buffer_adder_event,
			&pipe::on_buffer_consumer_event
		);
	}

}

