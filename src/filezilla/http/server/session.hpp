#ifndef FZ_HTTP_SERVER_SESSION_HPP
#define FZ_HTTP_SERVER_SESSION_HPP

#include "../../buffer_operator/streamed_adder.hpp"
#include "../../buffer_operator/file_reader.hpp"
#include "../../buffer_operator/file_writer.hpp"
#include "../../buffer_operator/tvfs_entries_lister.hpp"
#include "../../util/invoke_later.hpp"
#include "../../tvfs/engine.hpp"

#include "../../securable_socket.hpp"
#include "../../channel.hpp"

#include "../server.hpp"
#include "../message_consumer.hpp"
#include "../body_chunker.hpp"

#include "responder.hpp"
#include "request.hpp"

namespace fz::http {

class server::session
	: public tcp::session
	, private util::invoker_handler
	, private buffer_operator::streamed_adder
	, private message_consumer
	, private server::responder
	, private channel::progress_notifier
{
public:
	session(
		event_handler &target_handler, event_loop &loop, id id, std::unique_ptr<socket> socket, const securable_socket::info *security_info,
		transaction_handler &request_handler,
		logger_interface &logger
	);

	~session() override;

	void set_timeouts(const duration &keepalive_timeout, const duration &activity_timeout);
	bool is_secure() const;
	event_loop &get_event_loop() const;

	void receive_body(badge<server::request>, std::string &&body, std::function<void(std::string body, bool success)> on_end);
	void receive_body(badge<server::request>, tvfs::file_holder &&file, std::function<void(tvfs::file_holder file, bool success)> on_end);

	// progress_notifier interface
private:
	void notify_channel_socket_read_amount(const monotonic_clock &time_point, int64_t amount) override;
	void notify_channel_socket_written_amount(const monotonic_clock &time_point, int64_t amount) override;

	// consumer interface
private:
	int consume_buffer() override;

	// message_consumer interface
private:
	int process_message_start_line(std::string_view line) override;
	int process_message_header(field::name_view name, field::value_view value) override;
	int process_end_of_message_headers() override;
	int process_end_of_message() override;
	int process_error(int err, std::string_view msg) override;

	// event_handler interface
private:
	void operator ()(const event_base &) override;
	void on_socket_event(fz::socket_event_source *source, fz::socket_event_flag type, int error);
	void on_channel_done_event(channel &, channel::error_type error);
	void on_timer_event(timer_id id);

	// session interface
private:
	bool is_alive() const override;
	void shutdown(int err) override;

	// server::responder interface
private:
	using body_size_type = std::conditional_t<
		(std::numeric_limits<std::size_t>::max() > std::numeric_limits<std::uint64_t>::max()),
		std::size_t,
		std::uint64_t
	>;

	void flush_headers(body_size_type size_of_body);
	auto stream_headers(std::initializer_list<std::pair<field::name_view, field::value_view> > list);
	bool send_body(buffer_operator::adder_interface &adder);

	bool send_status(unsigned int code, std::string_view reason) override;
	bool send_headers(std::initializer_list<std::pair<field::name_view, field::value_view>>) override;
	bool send_body(std::string_view str) override;
	bool send_body(tvfs::file_holder file) override;
	bool send_body(tvfs::entries_iterator it) override;
	bool send_end() override;
	void abort_send(std::string_view msg) override;

private:
	transaction_handler &transaction_handler_;

	securable_socket::info security_info_;
	logger::modularized logger_;
	logger::modularized reqlog_;
	logger::modularized reslog_;

	securable_socket socket_;
	channel channel_;

	monotonic_clock last_activity_;
	duration keepalive_timeout_;
	duration activity_timeout_;
	timer_id keepalive_timer_id_{};
	timer_id activity_timer_id_{};

	void maybe_accept_next_request();

	struct transaction;
	std::shared_ptr<transaction> shared_transaction_;
	bool shared_transaction_must_be_made_{false};
};

}

#endif // FZ_HTTP_SERVER_SESSION_HPP
