#ifndef FZ_HTTP_SERVER_HPP
#define FZ_HTTP_SERVER_HPP

#include "../tcp/server.hpp"
#include "../securable_socket.hpp"

namespace fz::http {

class server
	: public tcp::server::delegate<server>
	, private tcp::session::factory::base
{
public:
	struct address_info;
	class session;
	class request;
	class responder;
	class transaction;
	class transaction_handler;

	using shared_transaction = std::shared_ptr<transaction>;

	server(tcp::server::context &context, event_loop_pool &event_loop_pool, transaction_handler &request_handler,
		   tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, logger_interface &logger);

	void set_security_info(const securable_socket::info &info);

	void set_timeouts(const duration &keepalive_timeout, const duration &activity_timeout);

private:
	std::unique_ptr<tcp::session> make_session(event_handler &target_handler, event_loop &loop, tcp::session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) override;

	fz::mutex mutex_;

	transaction_handler &transaction_handler_;
	logger::modularized logger_;
	tcp::server tcp_server_;
	securable_socket::info security_info_;

	duration keepalive_timeout_;
	duration activity_timeout_;
};

struct server::address_info: tcp::address_info
{
	bool use_tls = true;

	template <typename Archive>
	void serialize(Archive &ar)
	{
		tcp::address_info::serialize(ar);

		ar(FZ_NVP_O(use_tls));
	}

	tcp::listener::user_data get_user_data() const
	{
		if (use_tls) {
			return { true, "HTTPS" };
		}

		return { false, "HTTP" };
	}
};

}

#endif // FZ_HTTP_SERVER_HPP
