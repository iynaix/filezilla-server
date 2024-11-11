#ifndef FZ_RMP_ENGINE_CLIENT_IPP
#define FZ_RMP_ENGINE_CLIENT_IPP

#include "../../rmp/engine/client.hpp"
#include "../../rmp/engine/session.hpp"

namespace fz::rmp {

template <typename AnyMessage>
engine<AnyMessage>::client::client(thread_pool &pool, event_loop &loop, dispatcher &dispatcher, logger_interface &logger)
	: tcp::client(pool, loop, logger, *this)
	, dispatcher_(dispatcher)
	, logger_(logger)
{}

template <typename AnyMessage>
engine<AnyMessage>::client::~client()
{
	remove_handler();
}

template <typename AnyMessage>
void engine<AnyMessage>::client::connect(tcp::address_info address_info, bool use_tls)
{
	tcp::client::connect(std::move(address_info), use_tls);
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>>
engine<AnyMessage>::client::send(Args &&... args)
{
	fz::scoped_lock lock(mutex_);

	if (session_)
		static_cast<session &>(*session_).template send<Message>(std::forward<Args>(args)...);
}

template <typename AnyMessage>
template <typename Message>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message>>
engine<AnyMessage>::client::send(Message && m)
{
	fz::scoped_lock lock(mutex_);

	if (session_)
		static_cast<session &>(*session_).template send(std::forward<Message>(m));
}

template <typename AnyMessage>
void engine<AnyMessage>::client::enable_receiving(bool enabled)
{
	fz::scoped_lock lock(mutex_);

	if (session_)
		static_cast<session &>(*session_).enable_receiving(enabled);
}

template <typename AnyMessage>
void engine<AnyMessage>::client::set_certificate_verification_result(bool trusted)
{
	fz::scoped_lock lock(mutex_);

	if (session_)
		static_cast<session &>(*session_).set_certificate_verification_result(trusted);
}

template <typename AnyMessage>
std::unique_ptr<tcp::session> engine<AnyMessage>::client::make_session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */)
{
	auto use_tls = std::any_cast<bool>(&user_data);
	if (!use_tls) {
		// This should really never ever happen
		logger_.log(logmsg::error, L"User data is not of the proper type. This is an internal error.");
		error = EINVAL;
	}

	std::unique_ptr<session> p;

	if (socket && !error)
		p = std::make_unique<session>(
			target_handler,
			id,
			logger_,
			dispatcher_,
			std::move(socket),
			*use_tls,
			tls_ver::v1_2,
			typename session::tls_client_params{});
	else
		dispatcher_.connection(nullptr, error);

	return p;
}

}
#endif // FZ_RMP_ENGINE_CLIENT_IPP
