#ifndef FZ_RMP_ENGINE_CLIENT_HPP
#define FZ_RMP_ENGINE_CLIENT_HPP

#include <libfilezilla/logger.hpp>
#include <libfilezilla/thread_pool.hpp>

#include "../../tcp/client.hpp"

#include "../../rmp/engine.hpp"
#include "../../rmp/engine/dispatcher.hpp"

namespace fz::rmp {

template <typename AnyMessage>
class engine<AnyMessage>::client: public tcp::client, private tcp::session::factory
{
public:
	client(thread_pool &pool, event_loop &loop, dispatcher &dispatcher, logger_interface &logger);
	~client() override;

	void connect(tcp::address_info address_info, bool use_tls = true);

	template <typename Message, typename... Args>
	std::enable_if_t<trait::has_message_v<any_message, Message> && std::is_constructible_v<Message, Args...>>
	send(Args &&... args);

	template <typename Message>
	std::enable_if_t<trait::has_message_v<any_message, Message>>
	send(Message && m);

	void enable_receiving(bool enabled = true);

	void set_certificate_verification_result(bool trusted);

private:
	std::unique_ptr<tcp::session> make_session(event_handler &target_handler, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) override;

	dispatcher &dispatcher_;
	logger_interface &logger_;
};

}

#include "../../rmp/engine/client.ipp"

#endif // FZ_RMP_ENGINE_CLIENT_HPP
