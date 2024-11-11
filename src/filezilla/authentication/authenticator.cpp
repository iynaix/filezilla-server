#include "authenticator.hpp"

namespace fz::authentication {

void none_authenticator::authenticate(std::string_view, const methods_list &, address_type, std::string_view, event_handler &target, logger::modularized::meta_map)
{
	struct none_op: operation
	{
		shared_user get_user() override
		{
			return {};
		}

		available_methods get_methods() override
		{
			return {};
		}

		error get_error() override
		{
			return error::auth_method_not_supported;
		}

		bool next(const methods_list &) override
		{
			return false;
		}

		void stop() override
		{

		}
	};

	target.send_event<operation::result_event>(*this, std::unique_ptr<none_op>());
}

void none_authenticator::stop_ongoing_authentications(event_handler &)
{}

authenticator::operation::~operation()
{}

authenticator::~authenticator()
{
}

session_user::session_user()
	: error_(error::none)
{}

session_user::session_user(std::unique_ptr<authenticator::operation> op, logger_interface &logger)
{
	if (op) {
		su_ = op->get_user();
		error_ = op->get_error();

		if (su_) {
			if (auto u = su_->lock()) {
				if (u->session_count_limiter.limit_reached()) {
					logger.log_u(logmsg::error, L"User «%s» has reached the maximum allowed concurrent sessions limit (%d). Further authentication attempts denied until active sessions are released.", u->name, u->session_count_limiter.count());
					error_ = error::user_quota_reached;
				}
				else {
					for (auto &c: u->extra_session_count_limiters) {
						if (c->limit_reached()) {
							logger.log_u(logmsg::error, L"User «%s» has reached the maximum allowed concurrent sessions limit (%d) for %s. Further authentication attempts denied until active sessions are released.", u->name, c->count(), (!c->name().empty() ? c->name() : "«unknown»"));
							error_ = error::user_quota_reached;
							break;
						}
					}
				}

				if (!error_) {
					session_count_limiter_ = u->session_count_limiter;
					extra_session_count_limiters_.reserve(u->extra_session_count_limiters.size());
					for (auto &c: u->extra_session_count_limiters) {
						extra_session_count_limiters_.push_back(*c);
					}
				}
				else {
					su_.reset();
				}
			}
		}
		else
		if (!error_) {
			if (auto methods = op->get_methods(); methods.is_auth_necessary()) {
				logger.log_u(logmsg::error, L"Some auth methods still need verification, cannot authenticate user. Remaning auth methods [%s].", methods);
				error_ = error::auth_method_not_supported;
			}
		}

		stop(std::move(op));
	}
	else {
		logger.log_raw(logmsg::error, L"The authenticator operation is null, this is an internal error. Contact the administrator.");
		error_ = error::internal;
	}
}

}
