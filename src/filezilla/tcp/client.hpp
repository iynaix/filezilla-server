#ifndef FZ_TCP_CLIENT_HPP
#define FZ_TCP_CLIENT_HPP

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/logger.hpp>
#include <libfilezilla/thread_pool.hpp>
#include <libfilezilla/socket.hpp>

#include "../tcp/session.hpp"
#include "../util/invoke_later.hpp"

namespace fz::tcp {

class client: public util::invoker_handler
{
	FZ_UTIL_THREAD_CHECK_INIT

public:
	client(thread_pool &pool, event_loop &loop, logger_interface &logger, session::factory &session_factory);
	~client() override;

	void connect(tcp::address_info address_info, std::any user_data = {});
	void disconnect();
	bool is_connected() const;

private:
	void operator ()(const event_base &ev) override;
	void on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error);
	void on_session_ended_event(session::id p, const channel::error_type &);

	thread_pool &pool_;
	logger_interface &logger_;
	session::factory &session_factory_;

	std::unique_ptr<socket> socket_;
	address_info address_info_;
	std::any user_data_;

protected:
	mutable fz::mutex mutex_{};

	std::unique_ptr<session> session_;
};

}

#endif // FZ_TCP_CLIENT_HPP
