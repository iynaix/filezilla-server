#ifndef FZ_IMPERSONATOR_CHANNEL_HPP
#define FZ_IMPERSONATOR_CHANNEL_HPP

#include <memory>

#include <libfilezilla/socket.hpp>

#include "messages.hpp"

#include "../util/demangle.hpp"
#include "../util/locking_wrapper.hpp"
#include "../enum_bitops.hpp"
#include "../receiver/glue/rmp.hpp"

namespace fz::impersonator {

struct fdreadwriter: socket_event_source {
	using socket_event_source::socket_event_source;

	virtual void set_event_handler(event_handler *eh) = 0;

	virtual int send_fd(buffer &buf, fd_owner &fd) = 0;
	virtual int read_fd(buffer &buf, fd_owner &fd) = 0;
};

struct channel final: private event_handler
{
	enum ready_ops {
		can_send = 1,
		can_recv = 2
	};

	FZ_ENUM_BITOPS_FRIEND_DEFINE_FOR(ready_ops)

	struct ready_tag{};
	using ready = simple_event<ready_tag, ready_ops>;

	struct error_tag{};
	using error = simple_event<error_tag, int>;

	channel(event_loop &loop, logger_interface &logger, std::unique_ptr<fdreadwriter> rw);

	void close();

	~channel() override;

	void set_event_handler(event_handler *eh);

	int send(const any_message &);
	int recv(any_message &);

	explicit operator bool() const
	{
		return bool(rw_);
	}

private:
	int on_write();
	int on_read();
	void operator()(const event_base &ev) override;

	logger_interface &logger_;

	event_handler *eh_{};
	std::unique_ptr<fdreadwriter> rw_;

	bool has_in_msg_{};
	any_message in_msg_;

	buffer out_buf_;
	buffer in_buf_;

	fd_owner out_fd_;
	std::deque<fd_owner> in_fds_buf_;

	ready_ops must_send_ready_ops_{};
	bool waiting_for_read_event_{};
};

struct caller: private event_handler
{
	caller(event_loop &loop, logger_interface &logger, std::unique_ptr<fdreadwriter> rw, duration timeout = duration::from_seconds(10))
		: event_handler(loop)
		, logger_(logger)
		, timeout_(timeout)
		, channel_(loop, logger_, std::move(rw))
	{
		channel_.set_event_handler(this);
	}

	~caller() override;

	template <typename E>
	void call(any_message &&msg, receiver_handle<E> &&h)
	{
		return call(std::move(msg), std::move(h), any_message::type_index<rmp::make_message_t<E>>());
	}

	explicit operator bool() const
	{
		return bool(channel_);
	}

private:
	void call(any_message &&msg, receiver_handle_base &&h, std::size_t expected_msg_id);

	void on_error(int err);
	void on_ready(channel::ready_ops ops);
	void on_can_recv();
	void on_can_send();
	void on_timer(timer_id id);

	void operator()(const event_base &ev) override;

	struct res {
		receiver_handle_base receiver_handle_;
		std::size_t expected_in_msg_id_{};
		monotonic_clock deadline_{};
	};

	struct reqres
	{
		any_message out_msg_;
		res res_;
	};

	mutex mutex_;
	logger_interface &logger_;
	duration timeout_;
	timer_id timer_id_{};

	std::deque<reqres> send_queue_;
	std::deque<res> recv_queue_;

	bool waiting_for_can_send_event_{};

	channel channel_;
};

}
#endif // FZ_IMPERSONATOR_CHANNEL_HPP
