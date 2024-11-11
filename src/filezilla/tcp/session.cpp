#include <libfilezilla/util.hpp>

#include "../logger/type.hpp"
#include "session.hpp"

namespace fz::tcp {

session::session(event_handler &target_handler, id id, peer_info peer_info)
	: target_handler_(target_handler)
	, id_(id)
	, peer_info_(peer_info)
{
}

session::~session()
{
}

session::id session::get_id() const
{
	return id_;
}

session::peer_info session::get_peer_info() const
{
	return peer_info_;
}

session::factory::~factory()
{
}

void session::factory::listener_status_changed(const listener &)
{
}

bool session::factory::log_on_session_exit()
{
	return true;
}

bool session::factory::is_peer_allowed(std::string_view, address_type) const
{
	return true;
}

session::factory::base::base(event_loop_pool &pool, tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, logger_interface &logger, std::string name)
	: logger_(logger, std::move(name))
	, pool_(pool)
	, disallowed_ips_(disallowed_ips)
	, allowed_ips_(allowed_ips)
	, autobanner_(autobanner)
{
}

std::unique_ptr<session> session::factory::base::make_session(event_handler &target_handler, session::id id, std::unique_ptr<socket> socket, const std::any &user_data, int &error)
{
	if (!socket || error)
		return {};

	return make_session(target_handler, pool_.get_loop(), id, std::move(socket), user_data, error);
}

bool session::factory::base::is_peer_allowed(std::string_view ip, address_type family) const
{
	if (autobanner_.is_banned(ip, family)) {
		logger_.log_u(logmsg::warning, L"Address %s has been temporarily banned due to brute force protection. Refusing connection.", ip);
		return false;
	}

	if (disallowed_ips_.contains(ip, family)) {
		if (!allowed_ips_.contains(ip, family)) {
			logger_.log_u(logmsg::warning, L"Address %s has been banned. Refusing connection.", ip);
			return false;
		}
	}

	return true;
}

namespace {

struct none_notifier_factory: session::notifier::factory {
	std::unique_ptr<session::notifier> make_notifier(session::id, const datetime &, const std::string &, address_type, logger_interface &logger) override
	{
		struct none_notifier: session::notifier {
			none_notifier(logger_interface &logger): logger_(logger){}

			void notify_user_name(std::string_view) override {}
			void notify_entry_open(std::uint64_t, std::string_view, std::int64_t) override {}
			void notify_entry_close(std::uint64_t, int) override {}
			void notify_entry_write(std::uint64_t, std::int64_t, std::int64_t) override {}
			void notify_entry_read(std::uint64_t, std::int64_t, std::int64_t) override {}
			void notify_protocol_info(const protocol_info &) override{}

			logger_interface &logger() override { return logger_; }

		private:
			logger_interface &logger_;
		};

		return std::make_unique<none_notifier>(logger);
	}

	void listener_status(const tcp::listener &) override {}

} none_notifier_factory_instance;

}

session::notifier::factory &session::notifier::factory::none = none_notifier_factory_instance;

}
