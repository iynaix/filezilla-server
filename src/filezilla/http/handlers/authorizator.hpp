#ifndef FZ_HTTP_HANDLERS_AUTHORIZATOR_HPP
#define FZ_HTTP_HANDLERS_AUTHORIZATOR_HPP

#include <libfilezilla/event_handler.hpp>

#include "../server/transaction.hpp"
#include "../../authentication/user.hpp"
#include "../../authentication/authenticator.hpp"
#include "../../authentication/token_manager.hpp"

#include <libfilezilla/encryption.hpp>
#include <libfilezilla/signature.hpp>

namespace fz::http::handlers {

class authorizator: protected event_handler, public http::server::transaction_handler
{
public:
	struct custom_authorization_data_factory
	{
		virtual ~custom_authorization_data_factory();
		virtual std::shared_ptr<void> make_custom_authorization_data() = 0;
	};

	struct authorization;

	template <typename T = void>
	struct authorization_data
	{
		std::size_t id;
		authentication::shared_user user;
		std::shared_ptr<T> custom;

		template <typename U>
		std::optional<authorization_data<U>> as() const &
		{
			if (custom) {
				return authorization_data<U>{ id, user, std::static_pointer_cast<U>(custom) };
			}

			return std::nullopt;
		}

		template <typename U>
		std::optional<authorization_data<U>> as() &&
		{
			if (custom) {
				return authorization_data<U>{ id, std::move(user), std::static_pointer_cast<U>(std::move(custom)) };
			}

			return std::nullopt;
		}
	};

	authorizator(event_loop &loop, authentication::authenticator &auth, authentication::token_manager &tm, logger_interface &logger);

	~authorizator() override;

	[[nodiscard]] std::optional<authorization_data<>> get_authorization_data(const server::shared_transaction &t, custom_authorization_data_factory *adf);
	void authorize(const authentication::refresh_token &refresh_token, event_loop &loop, server::request &req, custom_authorization_data_factory *adf, std::function<void (std::optional<authorization_data<>>)> continuation);

	authentication::token_manager &get_token_manager()
	{
		return tm_;
	}

	void set_timeouts(duration access_token_timeout, duration refresh_token_timeout);

	void reset();

private:
	struct worker;

	[[nodiscard]] std::optional<std::string_view> get_access_token_bearer(server::request &req);
	[[nodiscard]] util::locked_proxy<authorization> get_authorization(std::string_view bearer);
	[[nodiscard]] util::locked_proxy<authorization> get_authorization(const authentication::access_token &access_token);
	[[nodiscard]] util::locked_proxy<authorizator::authorization> make_authorization(authentication::session_user session_user, authentication::refresh_token refresh_token = {});

	void operator()(const event_base &ev) override;

	void send_auth_error(server::responder &res, std::string_view error, std::string_view description = {});
	void send_auth_tokens(util::locked_proxy<authorization> authorization, std::string cookie_path, const server::shared_transaction &t);

	void do_token_password(std::string username, std::string password, std::string cookie_path, const server::shared_transaction &t);
	void do_token_refresh(std::string bearer, std::string cookie_path, const server::shared_transaction &t);

	void do_token(query_string q, const server::shared_transaction &t);
	void do_revoke(query_string q, const server::shared_transaction &t);
	void handle_transaction(const server::shared_transaction &t) override;

	mutable mutex mutex_;

	symmetric_key key_;
	authentication::authenticator &auth_;
	authentication::token_manager &tm_;

	logger::modularized logger_;

	std::unique_ptr<std::unordered_map<std::size_t, authorization>> authorizations_p_;
	std::unique_ptr<std::unordered_map<event_loop *, worker>> workers_p_;

	std::unordered_map<std::size_t, authorization> &authorizations_;
	std::unordered_map<event_loop *, worker> &workers_;

	duration access_token_timeout_ = duration::from_seconds(300);
	duration refresh_token_timeout_ = duration::from_days(15);
};

}

#endif // FZ_HTTP_HANDLERS_AUTHORIZATOR_HPP
