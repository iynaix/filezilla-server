#include <cassert>

#include "channel.hpp"

#include "../serialization/types/variant.hpp"
#include "../serialization/types/containers.hpp"
#include "../serialization/types/time.hpp"
#include "../serialization/types/local_filesys.hpp"
#include "../mpl/with_index.hpp"
#include "../mpl/for_each.hpp"
#include "../logger/type.hpp"
#include "../strsyserror.hpp"

namespace fz::impersonator {

channel::channel(event_loop &loop, logger_interface &logger, std::unique_ptr<fdreadwriter> rw)
	: event_handler(loop)
	, logger_(logger)
	, rw_(std::move(rw))
	, must_send_ready_ops_(can_send | can_recv)
{}

void channel::close()
{
	rw_.reset();
}

channel::~channel()
{
	remove_handler();
}

void channel::set_event_handler(event_handler *eh)
{
	if (eh_ == eh || !rw_)
		return;

	eh_ = eh;

	if (rw_)
		rw_->set_event_handler(eh_ ? this : nullptr);
}

int channel::send(const fz::impersonator::any_message &any)
{
	logger_.log_u(logmsg::debug_debug, L"Entering channel::send(%d)", any.index());

	if (!rw_) {
		logger_.log_raw(logmsg::error, L"on_write: fd readwriter not available. This should never happen.");
		return EBADF;
	}

	if (!out_buf_.empty()) {
		logger_.log_raw(logmsg::debug_debug, L"Previous message is still being sent. Only one at a time, please.");
		must_send_ready_ops_ |= can_send;
		return EAGAIN;
	}

	int err = output_archive(out_buf_, out_fd_)(any).error();
	if (err) {
		logger_.log_u(logmsg::error, L"send: could not serialize the message: %s (%d)", std::strerror(err), err);
		rw_.reset();

		return err;
	}

	logger_.log_u(logmsg::debug_debug, L"send: before on_write(): buf size: %d, out_fd = %d", out_buf_.size(), std::intptr_t(out_fd_.get()));

	err = on_write();

	logger_.log_u(logmsg::debug_debug, L"send: after on_write(): buf size: %d, err = %d", out_buf_.size(), err);

	return err;
}

int channel::on_write()
{
	if (!rw_) {
		logger_.log_raw(logmsg::warning, L"on_write: fd readwriter not available. This should never happen.");
		return 0;
	}

	int err = 0;

	while (!out_buf_.empty()) {
		if (err = rw_->send_fd(out_buf_, out_fd_); err)
			break;

		out_fd_ = fd_owner();
	}

	if (err == EAGAIN && eh_) {
		err = 0;
	}
	else
	if (err) {
		logger_.log_u(logmsg::error, L"on_write: send_fd failed. Reason: %s (%d)", std::strerror(err), err);

		rw_.reset();
	}

	return err;
}

int channel::recv(any_message &any)
{
	logger_.log_u(logmsg::debug_debug, L"entering channel::recv");

	if (!rw_) {
		logger_.log_raw(logmsg::error, L"recv: fd readwriter not available. This should never happen.");
		return EBADF;
	}

	if (!eh_) {
		int err = 0;
		fd_owner in_fd;

		const char *op = "";

		while (true) {
			op = "deserialization";
			err = input_archive(in_buf_, in_fds_buf_)(any).error();

			if (err == ENODATA) {
				if (err = rw_->read_fd(in_buf_, in_fd); err) {
					op = "read_fd";
					break;
				}

				if (in_fd)
					in_fds_buf_.push_back(std::move(in_fd));

				continue;
			}

			break;
		}

		if (err) {
			rw_.reset();
			logger_.log_u(logmsg::error, L"channel::recv: %s failed. Reason: %s (%d)", op, std::strerror(err), err);
		}

		return err;
	}


	int err = 0;

	if (!has_in_msg_)	{
		logger_.log_u(logmsg::debug_debug, L"channel::recv: deserialization: not enough data. Try again later.");
		must_send_ready_ops_ |= can_recv;

		err = EAGAIN;
	}
	else {
		logger_.log_u(logmsg::debug_debug, L"channel::recv: deserialization: got message %d. Err: %d", in_msg_.index(), err);
		any = std::move(in_msg_);
		has_in_msg_ = false;
	}

	if (!waiting_for_read_event_) {
		waiting_for_read_event_ = true;
		logger_.log_u(logmsg::debug_debug, L"sending socket read event: err: %d, can_recv: %d", err, !!(must_send_ready_ops_ & can_recv));
		send_event<socket_event>(rw_.get(), socket_event_flag::read, 0);
	}

	return err;
}

int channel::on_read()
{
	logger_.log_raw(logmsg::debug_debug, L"channel::on_read()");

	if (!rw_) {
		logger_.log_raw(logmsg::warning, L"on_read: fd readwriter not available. This should never happen.");
		return 0;
	}

	waiting_for_read_event_ = false;

	int err = 0;
	fd_owner in_fd;
	const char *op = "";

	while (true) {
		op = "deserialization";
		logger_.log_u(logmsg::debug_debug, L"channel::on_read: trying to deserialize: buf size: %d, fd buf size: %d", in_buf_.size(), in_fds_buf_.size());
		err = input_archive(in_buf_, in_fds_buf_)(in_msg_).error();
		logger_.log_u(logmsg::debug_debug, L"channel::on_read: tried to deserialize: err: %d, buf size: %d, fd buf size: %d", err, in_buf_.size(), in_fds_buf_.size());

		if (err == ENODATA) {
			if (err = rw_->read_fd(in_buf_, in_fd); err) {
				op = "read_fd";
				break;
			}

			if (in_fd)
				in_fds_buf_.push_back(std::move(in_fd));

			continue;
		}

		break;
	}

	if (err == EAGAIN) {
		logger_.log_u(logmsg::debug_debug, L"channel::recv: %s: EAGAIN.", op);
		waiting_for_read_event_ = true;
		err = 0;
	}
	else
	if (err) {
		rw_.reset();
		logger_.log_u(logmsg::error, L"channel::recv: %s failed. Reason: %s (%d)", op, std::strerror(err), err);
	}
	else
		has_in_msg_ = true;

	return err;
}

void channel::operator()(const event_base &ev)
{
	fz::dispatch<socket_event>(ev, [&](socket_event_source *, socket_event_flag flag, int err) {
		ready_ops ready_ops_{};

		logger_.log_u(logmsg::debug_debug, "Channel event: flag = %d", flag);

		if (!err && (flag & socket_event_flag::write)) {
			logger_.log_u(logmsg::debug_debug, "Channel event: invoking on_write(): buf size: %d", out_buf_.size());

			if (out_buf_.size())
				err = on_write();

			logger_.log_u(logmsg::debug_debug, "Channel event: invoked on_write(): buf size: %d, err: %d, can_send: %d", out_buf_.size(), err, !!(must_send_ready_ops_ & can_send));

			if (!err && out_buf_.empty() && (must_send_ready_ops_ & can_send)) {
				must_send_ready_ops_ &= ~can_send;
				ready_ops_ |= can_send;
			}
		}

		if (!err && (flag & socket_event_flag::read)) {
			logger_.log_u(logmsg::debug_debug, "Channel event: invoking on_read(): buf size: %d", in_buf_.size());

			err = on_read();

			logger_.log_u(logmsg::debug_debug, "Channel event: invoked on_read(): buf size: %d, err: %d, can_recv: %d, has_in_msg: %d, waiting_for_read_event: %d", in_buf_.size(), err, !!(must_send_ready_ops_ & can_recv), has_in_msg_, waiting_for_read_event_);

			if (!err && has_in_msg_ && (must_send_ready_ops_ & can_recv)) {
				must_send_ready_ops_ &= ~can_recv;
				ready_ops_ |= can_recv;
			}
		}

		assert(eh_ != nullptr);
		assert(err != EAGAIN);

		if (err)
			eh_->send_event<error>(err);
		else
		if (ready_ops_)
			eh_->send_event<ready>(ready_ops_);
	});
}

caller::~caller()
{
	remove_handler();
}

void caller::call(any_message &&msg, receiver_handle_base &&h, std::size_t expected_msg_id)
{
	scoped_lock lock(mutex_);

	send_queue_.push_back(reqres{std::move(msg), res{std::move(h), expected_msg_id}});

	if (!channel_) {
		logger_.log_raw(logmsg::error, L"caller::call: The internal channel is closed.");
		on_error(EBADF);
	}

	logger_.log_raw(logmsg::debug_debug, L"caller::call: Enqueued reqres.");

	if (send_queue_.size() == 1) {
		send_event<channel::ready>(channel::can_send);
		logger_.log_raw(logmsg::debug_debug, L"caller::call: sent channel::ready event.");
	}
	else
		logger_.log_raw(logmsg::debug_debug, L"caller::call: NOT sent channel::ready event, caller already waiting for it.");

	return;
}

void caller::on_error(int err)
{
	scoped_lock lock(mutex_);

	if (err)
		logger_.log_u(logmsg::error, L"Error: %s (%d). Closing channel.", std::strerror(err), err);

	stop_timer(timer_id_);
	timer_id_ = 0;
	channel_.close();

	auto send_default_response = [](res &res) {
		mpl::with_index<any_message::size()>(res.expected_in_msg_id_, [&](auto i) {
			using T = std::variant_alternative_t<i, any_message::variant>;
			if constexpr (!rmp::trait::is_any_exception_v<T>) {
				using E = make_receiver_event_t<T>;

				std::apply([&](auto &&... args) {
					res.receiver_handle_.execute<E>(std::forward<decltype(args)>(args)...);
				}, messages::default_for<T>()().tuple());
			}
		});
	};

	// We cannot satisfy the requests, hence respond with the default values provided by the backend interface.
	for (auto &r: recv_queue_)
		send_default_response(r);
	recv_queue_.clear();

	for (auto &r: send_queue_)
		send_default_response(r.res_);
	send_queue_.clear();
}

void caller::on_can_recv()
{
	any_message any;
	int error = 0;

	while ((error = channel_.recv(any)) == 0) {
		mpl::with_index<any.size()>(any.index(), [&](auto i) {
			using T = std::variant_alternative_t<i, any_message::variant>;

			logger_.log_u(logmsg::debug_info, L"on_can_recv: [%s] processing.", util::type_name<T>());

			if (recv_queue_.empty()) {
				logger_.log_raw(logmsg::debug_warning, L"This message is totally unexpected, no request has been made to warrant it!");
				return on_error(EINVAL);
			}

			auto &res = recv_queue_.front();

			if constexpr (rmp::trait::is_any_exception_v<T>) {
				std::get_if<rmp::any_exception>(&any)->handle([&](const rmp::exception::generic &e) {
					logger_.log_u(logmsg::error, L"Got exception: \"%s\"", e.description());
				});

				return on_error(EINVAL);
			}
			else {
				if (res.expected_in_msg_id_ != any.index()) {
					logger_.log_u(logmsg::error, L"[%s]: got unxpected message as response!", util::type_name<T>());
					return on_error(EBADMSG);
				}

				if (timer_id_) {
					logger_.log(logmsg::debug_debug, L"Stopping timeout timer with id %d.", timer_id_);

					auto deadline = recv_queue_.size() > 1 ? recv_queue_[1].deadline_ : monotonic_clock();
					timer_id_= stop_add_timer(timer_id_, deadline);

					if (timer_id_)
						logger_.log(logmsg::debug_debug, L"Added new timeout timer with id %d.", timer_id_);
				}

				auto &args = std::get_if<T>(&any)->tuple();

				if (logger_.should_log(logmsg::debug_info)) {
					mpl::for_each_v(args, [this](auto &v) {
						if constexpr (std::is_constructible_v<const fz::result &, decltype(v)>) {
							const fz::result &res = v;

							logger_.log_u(logmsg::debug_info, L"[%s]: message contains result: %d (raw = %d: %s)", util::type_name<T>(), res.error_, res.raw_, strsyserror(res.raw_));
							return false;
						}
						else {
							return true;
						}
					});

					logger_.log_u(logmsg::debug_info, L"[%s]: dispatching message", util::type_name<T>());
				}

				using E = make_receiver_event_t<T>;

				std::apply([&](auto &&... args) {
					res.receiver_handle_.execute<E>(std::forward<decltype(args)>(args)...);
				}, std::move(args));

				recv_queue_.pop_front();
			}
		});
	}

	if (error == EAGAIN)
		logger_.log_raw(logmsg::debug_debug, L"on_can_recv: got EAGAIN, waiting for next event.");
	else {
		logger_.log_raw(logmsg::error, L"on_can_recv: could not receive message from the server.");
		on_error(error);
	}
}

void caller::on_can_send()
{
	scoped_lock lock(mutex_);

	waiting_for_can_send_event_ = false;

	while (!send_queue_.empty()) {
		auto &req = send_queue_.front();

		int err = 0;

		if (!req.res_.receiver_handle_)
			logger_.log_raw(logmsg::debug_info, L"on_can_send: no receiver, no need to send.");
		else {
			err = channel_.send(req.out_msg_);

			if (err == EAGAIN) {
				logger_.log_raw(logmsg::debug_info, L"on_can_send: couldn't send msg right now, waiting for next event.");
				waiting_for_can_send_event_ = true;
				return;
			}

			if (timeout_) {
				req.res_.deadline_ = monotonic_clock::now() + timeout_;

				if (!timer_id_) {
					timer_id_ = add_timer(req.res_.deadline_);
					logger_.log(logmsg::debug_debug, L"Added timeout timer with id %d.", timer_id_);
				}
			}

			recv_queue_.push_back(std::move(req.res_));
		}

		send_queue_.pop_front();

		if (err) {
			logger_.log_raw(logmsg::error, L"on_can_send: couldn't send msg.");
			on_error(err);
			return;
		}
	}
}

void caller::on_ready(channel::ready_ops ops)
{
	logger_.log_u(logmsg::debug_debug, "caller::on_ready: ops: %d", ops);

	if (ops & channel::can_recv)
		on_can_recv();

	if (ops & channel::can_send)
		on_can_send();

}

void caller::on_timer(timer_id id)
{
	if (id != timer_id_) {
		logger_.log(logmsg::error, L"Got unexpected timer_id %d.", id);
		return on_error(EINVAL);
	}

	logger_.log_raw(logmsg::error, L"Timeout expired.");
	on_error(ETIMEDOUT);
}

void caller::operator()(const event_base &ev)
{
	fz::dispatch<
		channel::ready,
		channel::error,
		timer_event
	>(ev, this,
		&caller::on_ready,
		&caller::on_error,
		&caller::on_timer
	);
}

}
