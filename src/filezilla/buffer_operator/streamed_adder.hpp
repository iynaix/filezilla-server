#ifndef FZ_BUFFER_OPERATOR_STREAMED_ADDER_HPP
#define FZ_BUFFER_OPERATOR_STREAMED_ADDER_HPP

#include <optional>
#include <cassert>

#include "../buffer_operator/adder.hpp"
#include "../util/buffer_streamer.hpp"
#include "../logger/stdio.hpp"
#include "../util/dispatcher.hpp"

namespace fz::buffer_operator {

class streamed_adder: public adder {
public:
	int add_to_buffer() override {
		scoped_lock lock(mutex_);

		if (!event_sent_ && !nested_)
			return EAGAIN;

		event_sent_ = false;

		if (!nested_)
			return 0;

		auto &a = nested_->first;

		lock.unlock();

		int err = a.add_to_buffer();
		if (err == ENODATA || (err != 0 && err != EAGAIN && err != ENOBUFS)) {
			a.set_event_handler(nullptr);

			lock.lock();
			auto eof_handler = std::move(nested_->second);
			nested_.reset();
			lock.unlock();

			if (err == ENODATA) {
				err = 0;
			}

			if (eof_handler) {
				err = eof_handler(err);
			}
		}

		return err;
	}

	util::buffer_streamer buffer_stream() {
		assert(!nested_ && "You must not invoke buffer_stream() when processing a nested adder.");

		auto proxy = get_buffer();

		auto &buffer = *proxy;

		// The proxy will be moved onto proxy_storage for the whole buffer_streamer lifecycle, then at
		// the end of the lambda it will be disposed of, unlocking the buffer again.
		proxy_storage_.emplace(std::move(proxy));

		return {buffer, [this]() {
			scoped_lock lock(mutex_);

			if (!event_sent_) {
				event_sent_ = adder::send_event(0);
			}

			proxy_storage_.reset();
		}};
	}

	void set_event_handler(event_handler *eh) override {
		adder::set_event_handler(eh);

		scoped_lock lock(mutex_);

		if (nested_)
			nested_->first.set_event_handler(eh);
	}

	void process_nested_adder_until_eof(adder_interface &a, std::function<int(int)> eof_handler = {})
	{
		{
			scoped_lock lock(mutex_);

			assert(!nested_ && "Already processing a nested adder.");
			if (nested_)
				return;

			nested_.emplace(a, std::move(eof_handler));

		}

		copy_buffer_to(a);
		a.set_event_handler(get_event_handler().get());

		{
			scoped_lock lock(mutex_);

			if (!event_sent_) {
				event_sent_ = adder::send_event(0);
			}
		}

	}

	int stop_processing_nested_adder()
	{
		scoped_lock lock(mutex_);

		if (nested_) {
			nested_->first.set_buffer(nullptr);
			nested_->first.set_event_handler(nullptr);

			auto eof_handler = std::move(nested_->second);
			nested_.reset();
			lock.unlock();

			if (eof_handler) {
				return eof_handler(0);
			}
		}

		return 0;
	}

private:
	mutex mutex_;
	bool event_sent_{false};
	std::optional<util::locked_proxy<locking_buffer::proxy>> proxy_storage_;
	std::optional<std::pair<adder_interface &, std::function<int(int)>>> nested_;
};

}

#endif // FZ_BUFFER_OPERATOR_STREAMED_ADDER_HPP
