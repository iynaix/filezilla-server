#ifndef FZ_HTTP_HANDLERS_AUTHORIZATOR_AUTHORIZATION_HPP
#define FZ_HTTP_HANDLERS_AUTHORIZATOR_AUTHORIZATION_HPP

#include "../authorizator.hpp"

namespace fz::http::handlers
{

struct authorizator::authorization: protected event_handler
{
	using expired_event = simple_event<authorization, authorization &>;

	authorization(authentication::session_user session_user, authentication::refresh_token refresh_token, authorizator &owner);
	authorization(authorization &&) = delete;

	~authorization() override;

	std::optional<authorization_data<>> get_data(custom_authorization_data_factory *adf = nullptr);

	const authentication::refresh_token &get_refresh_token() const
	{
		return refresh_token_;
	}

	void set_session_user(authentication::session_user session_user);

private:
	void expire();
	void set(authentication::session_user session_user, authentication::refresh_token refresh_token);

	void operator()(const event_base &ev) override;

	void on_timer(timer_id);
	void on_user_changed(authentication::weak_user &u);

	expired_event expired_event_{*this};
	authorizator &owner_;

	authentication::refresh_token refresh_token_{};
	authentication::session_user session_user_{};
	timer_id timer_id_{};

	std::unordered_map<custom_authorization_data_factory*, std::shared_ptr<void>> data_;
};

}

#endif // FZ_HTTP_HANDLERS_AUTHORIZATOR_AUTHORIZATION_HPP
