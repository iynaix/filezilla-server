#ifndef FZ_TCP_SESSION_HPP
#define FZ_TCP_SESSION_HPP

#include <libfilezilla/event_handler.hpp>
#include <libfilezilla/logger.hpp>
#include <libfilezilla/file.hpp>

#include "listener.hpp"
#include "address_list.hpp"
#include "../logger/modularized.hpp"
#include "../channel.hpp"
#include "../authentication/autobanner.hpp"
#include "../event_loop_pool.hpp"

namespace fz::tcp {

class session
{
public:
	using id = std::uint64_t;
	using peer_info = std::pair<std::string, address_type>;

	class factory;
	class notifier;

	using ended_event = simple_event<session, id, channel::error_type /*error*/>;

public:
	session(event_handler &target_handler, id id, peer_info peer_info);
	virtual ~session();

	session(session &&) = delete;
	session(const session &) = delete;

	id get_id() const;
	peer_info get_peer_info() const;

	virtual bool is_alive() const = 0;

	//! Must shut the session down and send an ended_event when done.
	//! Postcondition: is_alive() == false;
	virtual void shutdown(int err = 0) = 0;

protected:
	event_handler &target_handler_;
	id id_;
	peer_info peer_info_;
};

class session::factory: public listener::peer_allowance_checker, public listener::status_change_notifier
{
public:
	class base;

	virtual ~factory() override;
	virtual std::unique_ptr<session> make_session(event_handler &target_handler, session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) = 0;
	virtual void listener_status_changed(const listener &listener) override;
	virtual bool log_on_session_exit();
	virtual bool is_peer_allowed(std::string_view ip, address_type family) const override;
};

class session::factory::base: public session::factory
{
public:
	base(event_loop_pool &pool, tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, logger_interface &logger, std::string name);

	std::unique_ptr<session> make_session(event_handler &target_handler, session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) override final;
	virtual std::unique_ptr<session> make_session(event_handler &target_handler, event_loop &loop, session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error /* In-Out */) = 0;

	bool is_peer_allowed(std::string_view ip, address_type family) const override final;

private:
	fz::mutex mutex_;
	mutable logger::modularized logger_;

	event_loop_pool &pool_;
	tcp::address_list &disallowed_ips_;
	tcp::address_list &allowed_ips_;
	authentication::autobanner &autobanner_;

};

class session::notifier {
public:
	class factory;

	struct protocol_info
	{
		enum status
		{
			insecure,
			not_completely_secure,
			secure
		};

		virtual ~protocol_info(){}

		virtual status get_status() const = 0;
		virtual std::string get_name() const = 0;

	public:
		protocol_info() = default;
		protocol_info(const protocol_info &) = default;
		protocol_info(protocol_info &&) = default;
		protocol_info &operator=(const protocol_info &) = default;
		protocol_info &operator=(protocol_info &&) = default;
	};

	virtual ~notifier(){}

	virtual void notify_user_name(std::string_view name) = 0;

	virtual void notify_entry_open(std::uint64_t id, std::string_view path, std::int64_t size) = 0;
	virtual void notify_entry_close(std::uint64_t id, int error) = 0;
	virtual void notify_entry_write(std::uint64_t id, std::int64_t amount, std::int64_t size) = 0;
	virtual void notify_entry_read(std::uint64_t id, std::int64_t amount, std::int64_t offset) = 0;

	virtual void notify_protocol_info(const protocol_info &info) = 0;

	virtual logger_interface &logger() = 0;
};

class session::notifier::factory
{
public:
	virtual ~factory() = default;
	virtual std::unique_ptr<notifier> make_notifier(session::id id, const datetime &start, const std::string &peer_ip, address_type peer_address_type, logger_interface &logger) = 0;
	virtual void listener_status(const tcp::listener &listener) = 0;

	static factory &none;
};
}

#endif // FZ_TCP_PEER_HPP
