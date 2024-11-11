#include "authorization.hpp"

namespace fz::http::handlers
{

authorizator::authorization::authorization(authentication::session_user session_user, authentication::refresh_token refresh_token, authorizator &owner)
	: event_handler(owner.event_loop_)
	, owner_(owner)
{
	set(std::move(session_user), std::move(refresh_token));
}

authorizator::authorization::~authorization()
{
	remove_handler();
	unsubscribe(session_user_, *this);
}

std::optional<authorizator::authorization_data<>> authorizator::authorization::get_data(custom_authorization_data_factory *adf)
{
	scoped_lock lock(owner_.mutex_);

	if (!refresh_token_) {
		return std::nullopt;
	}

	if (!timer_id_) {
		return std::nullopt;
	}

	authentication::shared_user shared_user = session_user_;

	if (!shared_user) {
		return std::nullopt;
	}

	if (auto u = shared_user->lock(); !u || u->id.empty()) {
		return std::nullopt;
	}

	auto c = [&]() -> std::shared_ptr<void> {
		if (adf) {
			auto &d = data_[adf];
			if (!d) {
				d = adf->make_custom_authorization_data();
			}

			return d;
		}

		return {};
	}();

	return authorization_data<>{ refresh_token_.access.id, std::move(shared_user), std::move(c) };
}

void authorizator::authorization::set_session_user(authentication::session_user session_user)
{
	if (!session_user) {
		owner_.logger_.log_u(logmsg::debug_info, L"Authorization with id (%d,%d) for user [%s] has been nullified.", refresh_token_.access.id, refresh_token_.access.refresh_id, refresh_token_.username);
		return expire();
	}

	auto refresh_token = owner_.tm_.refresh(refresh_token_);
	set(std::move(session_user), std::move(refresh_token));
}

void authorizator::authorization::set(authentication::session_user session_user, authentication::refresh_token refresh_token)
{
	scoped_lock lock(owner_.mutex_);

	stop_timer(std::exchange(timer_id_, 0));

	if (!session_user) {
		owner_.logger_.log_u(logmsg::debug_info, L"Authorization with id (%d,%d) for user [%s] has been nullified.", refresh_token_.access.id, refresh_token_.access.refresh_id, refresh_token_.username);
		return expire();
	}

	if (!refresh_token) {
		owner_.logger_.log_u(logmsg::error, L"Error while refreshing token for authorization with id (%d,%d) for user [%s].", refresh_token_.access.id, refresh_token_.access.refresh_id, refresh_token_.username);
		return expire();
	}

	unsubscribe(session_user_, *this);

	refresh_token_ = std::move(refresh_token);
	session_user_ = std::move(session_user);
	subscribe(session_user_, *this);

	timer_id_ = add_timer(owner_.access_token_timeout_, true);
	return;
}

void authorizator::authorization::expire()
{
	stop_timer(std::exchange(timer_id_, 0));

	unsubscribe(session_user_, *this);

	owner_.send_persistent_event(&expired_event_);
}

void authorizator::authorization::operator()(const event_base &ev)
{
	fz::dispatch<
		authentication::shared_user_changed_event,
		timer_event
	>(ev, this,
		&authorization::on_user_changed,
		&authorization::on_timer
	);
}

void authorizator::authorization::on_timer(timer_id)
{
	owner_.logger_.log_u(logmsg::debug_info, L"Authorization with id (%d,%d) for user [%s] has expired.", refresh_token_.access.id, refresh_token_.access.refresh_id, refresh_token_.username);
	expire();
}

void authorizator::authorization::on_user_changed(authentication::weak_user &wu)
{
	if (auto su = wu.lock()) {
		if (auto u = su->lock()) {
			if (u->id.empty()) {
				owner_.logger_.log_u(logmsg::debug_info, L"Authorization with id (%d,%d) for user [%s] has been terminated.", refresh_token_.access.id, refresh_token_.access.refresh_id, refresh_token_.username);
				owner_.send_persistent_event(&expired_event_);
			}
		}
	}
}

}
