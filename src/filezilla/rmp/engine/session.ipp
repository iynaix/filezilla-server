#ifndef FZ_RMP_ENGINE_PEER_IPP
#define FZ_RMP_ENGINE_PEER_IPP

#include <libfilezilla/tls_info.hpp>

#include "../../rmp/engine/session.hpp"
#include "../../rmp/engine/dispatcher.hpp"

namespace fz::rmp {

template <typename AnyMessage>
engine<AnyMessage>::session::session(event_handler &target_handler, tcp::session::id id,
									 logger_interface &logger, dispatcher &dispatcher,
									 std::unique_ptr<socket> socket)
	: tcp::session(target_handler, id, {socket->peer_ip(), socket->address_family()})
	, invoker_handler(target_handler.event_loop_)
	, buffer_operator::serialized_adder(4*128*1024) //<- this should be small enough to be responsive, but not too small or it will overflow very fast
	, buffer_operator::serialized_consumer()
	, logger_(logger, "RMP Session", {{"id", std::to_string(id)}})
	, dispatcher_(dispatcher)
	, socket_(target_handler.event_loop_, nullptr, std::move(socket), logger)
	, channel_(*this, 4*128*1024, 5, true)
{
	socket_.set_buffer_sizes(4*128*1024, 4*128*1024);
	socket_.set_flags(socket::flag_keepalive | socket::flag_nodelay);
	socket_.set_keepalive_interval(duration::from_seconds(30));

	channel_.set_buffer_adder(this);
	channel_.set_buffer_consumer(this);
}

template <typename AnyMessage>
engine<AnyMessage>::session::session(event_handler &target_handler, tcp::session::id id,
									 logger_interface &logger, dispatcher &dispatcher,
									 std::unique_ptr<socket> socket, bool use_tls, tls_ver min_tls_ver,
									 tls_server_params && tls_server_params)
	: session(target_handler, id, logger, dispatcher, std::move(socket))
{
	invoke_later([this, use_tls, min_tls_ver, tls_server_params = std::move(tls_server_params)] {
		if (use_tls) {
			socket_.set_event_handler(this);

			if (!socket_.make_secure_server(min_tls_ver, tls_server_params.cert_info)) {
				logger_.log_u(logmsg::error, L"socket_.make_secure_server() failed. Shutting down.");

				channel_.set_socket(&socket_);
				channel_.shutdown(EPROTO);
				return;
			}
		}
		else {
			channel_.set_socket(&socket_);
			dispatcher_.connection(this, 0);
		}
	});
}

template <typename AnyMessage>
engine<AnyMessage>::session::session(event_handler &target_handler, tcp::session::id id,
									 logger_interface &logger, dispatcher &dispatcher,
									 std::unique_ptr<socket> socket, bool use_tls, tls_ver min_tls_ver,
									 tls_client_params &&tls_client_params)
	: session(target_handler, id, logger, dispatcher, std::move(socket))
{
	invoke_later([this, use_tls, min_tls_ver, tls_client_params = std::move(tls_client_params)] {
		if (use_tls) {
			socket_.set_event_handler(this);

			if (!socket_.make_secure_client(min_tls_ver, tls_client_params.cert_info, tls_client_params.trust_store)) {
				logger_.log_u(logmsg::error, L"socket_.make_secure_server() failed. Shutting down.");

				channel_.set_socket(&socket_);
				channel_.shutdown(EPROTO);
				return;
			}
		}
		else {
			channel_.set_socket(&socket_);
			dispatcher_.connection(this, 0);
		}
	});
}

template <typename AnyMessage>
template <typename Message>
struct engine<AnyMessage>::session::serialized_data: buffer_operator::serialized_adder_data<void>
{
	using buffer_operator::serialized_adder_data<void>::serialized_adder_data;

protected:
	friend class session;
};

template <typename AnyMessage>
engine<AnyMessage>::session::~session()
{
	remove_handler();
/*
	if (is_alive())
		dispatcher_.disconnection(*this, 0);*/
}

template <typename AnyMessage>
bool engine<AnyMessage>::session::is_alive() const
{
	return channel_.get_socket() != nullptr;
}

template <typename AnyMessage>
void engine<AnyMessage>::session::shutdown(int err)
{
	channel_.shutdown(err);
}

template <typename AnyMessage>
std::string engine<AnyMessage>::session::peer_ip() const
{
	return socket_.peer_ip();
}

template <typename AnyMessage>
int engine<AnyMessage>::session::peer_port() const
{
	int error;
	return socket_.peer_port(error);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::set_certificate_verification_result(bool trusted)
{
	socket_.set_verification_result(trusted);
}

template <typename AnyMessage>
template <typename... Ts>
std::enable_if_t<(mpl::contains_v<typename engine<AnyMessage>::any_message, Ts> && ...)>
engine<AnyMessage>::session::enable_sending(bool enabled)
{
	enabled_sending_mask_.template set<Ts...>(enabled);
}

template <typename AnyMessage>
template <typename... Ts>
std::enable_if_t<(mpl::contains_v<typename engine<AnyMessage>::any_message, Ts> && ...)>
engine<AnyMessage>::session::enable_dispatching(bool enabled)
{
	enabled_dispatching_mask_.template set<Ts...>(enabled);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::enable_receiving(bool enabled)
{
	enable_consuming(enabled);
}

template <typename AnyMessage>
template <typename T>
void engine<AnyMessage>::session::set_user_data(T && data)
{
	user_data_ = std::forward<T>(data);
}

template <typename AnyMessage>
template <typename T>
T& engine<AnyMessage>::session::get_user_data()
{
	return *std::any_cast<T>(&user_data_);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::on_certificate_verification_event(tls_layer *, tls_session_info &info)
{
	dispatcher_.certificate_verification(*this, std::move(info));
}

template <typename AnyMessage>
void engine<AnyMessage>::session::on_socket_event(fz::socket_event_source *source, fz::socket_event_flag type, int error)
{
	channel_.set_socket(&socket_);

	if (error && source->root() == socket_.root()) {
		logger_.log_u(logmsg::error, L"Failed securing connection. Reason: %s.", socket_error_description(error));

		channel_.shutdown(error);
		return;
	}

	if (type == socket_event_flag::connection) {
		if (source != source->root() && source->root() == socket_.root()) {
			// All fine, hand the socket down to the channel.
			dispatcher_.connection(this, 0);
			return;
		}
	}

	logger_.log_u(logmsg::error,
				L"We got an unexpected socket_event. is_own_socket [%d], flag [%d], error [%d], from a layer [%d]",
				source->root() == socket_.root(), type, error, source != source->root());

	channel_.shutdown(EINVAL);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::on_channel_done_event(channel &, channel::error_type error)
{
	dispatcher_.disconnection(*this, error);
	target_handler_.send_event<ended_event>(id_, error);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::operator ()(const event_base &ev)
{
	if (on_invoker_event(ev))
		return;

	fz::dispatch<
		channel::done_event,
		fz::socket_event,
		fz::certificate_verification_event
	>(ev, this,
		&session::on_channel_done_event,
		&session::on_socket_event,
		&session::on_certificate_verification_event
	);
}

template <typename AnyMessage>
template <typename Message>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message>, int>
engine<AnyMessage>::session::send(const serialized_data<Message> &data)
{
	const bool enabled = enabled_sending_mask_.template test<Message>(true);

	FZ_RMP_DEBUG_LOG(L"send(const serialized_data<%s>): enabled: %d", util::type_name<Message>(), enabled);

	if (!enabled)
		return 0;

	return serialize_to_buffer(data);
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>, int>
engine<AnyMessage>::session::send(Args &&... args)
{
	const bool enabled = enabled_sending_mask_.template test<Message>(true);

	FZ_RMP_DEBUG_LOG(L"send<%s>(args...): enabled: %d", util::type_name<Message>(), enabled);

	if (!enabled)
		return 0;

	auto i = std::uint16_t(any_message::template type_index<Message>());
	Message m(std::forward<Args>(args)...);

	return serialize_to_buffer(serialization::nvp(i, "message_index"), serialization::nvp(m, "message"));
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>, int>
engine<AnyMessage>::session::send(serialized_data<Message> &data, Args &&... args)
{
	const bool enabled = enabled_sending_mask_.template test<Message>(true);

	FZ_RMP_DEBUG_LOG(L"send(serialized_data<%s>, args...): enabled: %d", util::type_name<Message>(), enabled);

	if (!enabled)
		return 0;

	if (!data)
		get_serialized_data(data, std::forward<Args>(args)...);

	return serialize_to_buffer(data);
}

template <typename AnyMessage>
template <typename Message>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message>, int>
engine<AnyMessage>::session::send(const Message &m)
{
	const bool enabled = enabled_sending_mask_.template test<Message>(true);

	FZ_RMP_DEBUG_LOG(L"send(%s): enabled: %d", util::type_name<Message>(), enabled);

	if (!enabled)
		return 0;

	auto i = std::uint16_t(any_message::template type_index<Message>());
	return serialize_to_buffer(serialization::nvp(i, "message_index"), serialization::nvp(m, "message"));
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>>
engine<AnyMessage>::session::get_serialized_data(serialized_data<Message> &data, Args &&... args)
{
	auto i = std::uint16_t(any_message::template type_index<Message>());
	Message m(std::forward<Args>(args)...);

	FZ_RMP_DEBUG_LOG(L"get_serialized_data<%s>: idx: %d, data.size: %d", util::type_name<Message>(), i, data.buffer.size());

	buffer_operator::serialized_adder::get_serialized_data(data, serialization::nvp(i, "message_index"), serialization::nvp(m, "message"));
}

template <typename AnyMessage>
void engine<AnyMessage>::session::set_max_buffer_size(std::size_t max)
{
	channel_.set_max_buffer_size(max);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::serialized_adder_buffer_overflow()
{
	dispatcher_.send_buffer_is_in_overflow(*this);
}

template <typename AnyMessage>
void engine<AnyMessage>::session::process_buffer_payload(serialization::binary_input_archive &l)
{
	if (std::uint16_t i; l(serialization::nvp(i, "message_index"))) {
		const bool enabled = i == 0 || enabled_dispatching_mask_.test(i-1);

		FZ_RMP_DEBUG_LOG(L"process_buffer_payload(): idx: %d, enabled: %d", i, enabled);

		if (enabled)
			dispatcher_.load_and_dispatch(*this, i, l);
	}
	else {
		FZ_RMP_DEBUG_LOG(L"process_buffer_payload(): deserialization failed. error: %d", (int)l.error());
	}

	if (int error = l.error()) {
		// We don't want to send out any exception if the error is negative.
		if (error < 0)
			// And we need to positivize it before passing it out to the outer layer.
			l.error(-error);
		else {
			send<any_exception>(exception::serialization_error(error));
			l.error(0);
		}
	}
}

}

#endif // FZ_RMP_ENGINE_PEER_IPP
