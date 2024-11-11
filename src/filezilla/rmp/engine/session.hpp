#ifndef FZ_RMP_ENGINE_SESSION_HPP
#define FZ_RMP_ENGINE_SESSION_HPP

#include <any>

#include <libfilezilla/event_handler.hpp>

#include "../../logger/modularized.hpp"
#include "../../buffer_operator/serialized_consumer.hpp"
#include "../../buffer_operator/serialized_adder.hpp"
#include "../../channel.hpp"
#include "../../securable_socket.hpp"
#include "../../util/invoke_later.hpp"
#include "../../util/typemask.hpp"

#include "../../tcp/session.hpp"
#include "../../rmp/engine.hpp"

namespace fz::rmp {

template <typename AnyMessage>
class engine<AnyMessage>::session
	: public tcp::session
	, public util::invoker_handler
	, private buffer_operator::serialized_adder
	, private buffer_operator::serialized_consumer
{
public:
	struct tls_server_params {
		const securable_socket::cert_info &cert_info;
	};

	struct tls_client_params{
		const securable_socket::cert_info *cert_info{};
		tls_system_trust_store *trust_store{};
	};

	session(event_handler &target_handler, tcp::session::id id,
			logger_interface &logger, dispatcher &dispatcher,
			std::unique_ptr<socket> socket, bool use_tls, tls_ver min_tls_ver,
			tls_server_params && tls_server_params);

	session(event_handler &target_handler, tcp::session::id id,
			logger_interface &logger, dispatcher &dispatcher,
			std::unique_ptr<socket> socket, bool use_tls, tls_ver min_tls_ver,
			tls_client_params && tls_client_params);

	~session() override;

	template <typename Message>
	struct serialized_data;

	template <typename Message>
	std::enable_if_t<trait::has_message_v<any_message, Message>, int>
	send(const serialized_data<Message> &);

	template <typename Message, typename... Args>
	std::enable_if_t<trait::has_message_v<any_message, Message> && std::is_constructible_v<Message, Args...>, int>
	send(Args &&... args);

	template <typename Message, typename... Args>
	std::enable_if_t<trait::has_message_v<any_message, Message> && std::is_constructible_v<Message, Args...>, int>
	send(serialized_data<Message> &, Args &&... args);

	template <typename Message>
	std::enable_if_t<trait::has_message_v<any_message, Message>, int>
	send(const Message &m);

	bool is_alive() const override;
	void shutdown(int err = 0) override;
	std::string peer_ip() const;
	int peer_port() const;

	template <typename... Ts>
	std::enable_if_t<(mpl::contains_v<any_message, Ts> && ...)>
	enable_sending(bool enabled = true);

	template <typename... Ts>
	std::enable_if_t<(mpl::contains_v<any_message, Ts> && ...)>
	enable_dispatching(bool enabled = true);

	void enable_receiving(bool enabled = true);

	template <typename T>
	void set_user_data(T &&);

	template <typename T>
	T &get_user_data();

	void set_certificate_verification_result(bool trusted);

	template <typename Message, typename... Args>
	std::enable_if_t<trait::has_message_v<any_message, Message> && std::is_constructible_v<Message, Args...>>
	static get_serialized_data(serialized_data<Message> &data, Args &&... args);

	void set_max_buffer_size(std::size_t max);

private:
	void serialized_adder_buffer_overflow() override;

private:
	void on_certificate_verification_event(tls_layer *, tls_session_info &info);
	void on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error);
	void on_channel_done_event(channel &, channel::error_type error);
	void operator ()(const event_base &ev) override;
	void process_buffer_payload(serialization::binary_input_archive &loader) override;

private:
	session(event_handler &target_handler, tcp::session::id id,
			logger_interface &logger, dispatcher &dispatcher,
			std::unique_ptr<socket> socket);

	logger::modularized logger_;
	dispatcher &dispatcher_;
	securable_socket socket_;
	channel channel_;

	util::typemask<any_message> enabled_sending_mask_;
	util::typemask<any_message> enabled_dispatching_mask_;

	std::any user_data_;
};

}

#include "../../rmp/engine/session.ipp"

#endif // FZ_RMP_ENGINE_SESSION_HPP
