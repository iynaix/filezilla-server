#ifndef FZ_RMP_ENGINE_SERVER_IPP
#define FZ_RMP_ENGINE_SERVER_IPP

#include <libfilezilla/util.hpp>

#include "../../rmp/engine/server.hpp"
#include "../../rmp/engine/session.hpp"

namespace fz::rmp {

template <typename AnyMessage>
engine<AnyMessage>::server::server(fz::tcp::server::context &context, dispatcher &dispatcher, logger_interface &logger)
	: tcp::server::delegate<server>(tcp_server_)
	, tcp_server_(context, logger, *this)
	, dispatcher_(dispatcher)
	, logger_(logger)
{}

template <typename AnyMessage>
void engine<AnyMessage>::server::server::set_security_info(const securable_socket::info &security_info)
{
	security_info_ = security_info;
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>, int>
engine<AnyMessage>::server::broadcast(Args &&... args)
{
	typename session::template serialized_data<Message> serialized_data;
	int err = ENOTCONN;

	tcp_server_.iterate_over_sessions([&](tcp::session &tcp_session) {
		auto &rmp_session = static_cast<engine::session &>(tcp_session);

		if (tcp_server_.get_number_of_sessions() == 1) {
			// Avoid unnecessary copy into intermediate buffer (held by serialized_data) if only one admin is connected.
			err = rmp_session.template send<Message>(std::forward<Args>(args)...);
			return !err;
		}
		else {
			rmp_session.send(serialized_data, std::forward<Args>(args)...);
			err = serialized_data.error();
			return !err;
		}
	});

	return err;
}

template <typename AnyMessage>
template <typename Message, typename... Args>
std::enable_if_t<trait::has_message_v<typename engine<AnyMessage>::any_message, Message> && std::is_constructible_v<Message, Args...>, std::pair<int, typename engine<AnyMessage>::session::id>>
engine<AnyMessage>::server::send_to_random_client(Args &&... args)
{
	int err = ENOTCONN;
	typename session::id id = 0;

	tcp_server_.iterate_over_sessions([&, elected = size_t(-1)](tcp::session &tcp_session) mutable {
		auto &rmp_session = static_cast<engine::session &>(tcp_session);

		if (elected == size_t(-1))
			elected = fz::random_number(0, tcp_server_.get_number_of_sessions()-1);

		if (elected-- == 0) {
			err = rmp_session.template send<Message>(std::forward<Args>(args)...);
			id = rmp_session.get_id();
			return false;
		}

		return true;
	});

	return {err, id};
}


template <typename AnyMessage>
std::unique_ptr<tcp::session> engine<AnyMessage>::server::make_session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */)
{
	auto use_tls = std::any_cast<bool>(&user_data);
	if (!use_tls) {
		// This should really never ever happen
		logger_.log(logmsg::error, L"User data is not of the proper type. This is an internal error.");
		error = EINVAL;
	}

	std::unique_ptr<session> p;

	if (socket && !error) {
		#if !defined (ENABLE_ADMIN_DEBUG) || !defined(ENABLE_ADMIN_DEBUG)
			const static bool s_use_tls = true;
			use_tls = &s_use_tls;
		#endif

		p = std::make_unique<session>(
			target_handler, id,
			logger_, dispatcher_,
			std::move(socket), *use_tls, security_info_.min_tls_ver, typename session::tls_server_params{security_info_.cert}
		);
	}

	return p;
}

inline static thread_local fz::buffer serialized_data_buffer_;

}

#endif
