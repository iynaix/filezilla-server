#include <libfilezilla/json.hpp>
#include <libfilezilla/encryption.hpp>

#include "authorizator.hpp"

#include "../server/responder.hpp"

#include "authorizator/authorization.hpp"

namespace fz::http::handlers
{

struct authorizator::worker: event_handler
{
	using continuation_type = std::function<void(authentication::session_user session_user)>;

	worker(authorizator &a, event_loop &loop)
		: event_handler(loop)
		, a_(a)
	{
	}

	void authenticate(std::string_view username, std::string_view password, http::server::request &req, http::server::shared_transaction t, continuation_type continuation)
	{
		t_ = std::move(t);
		continuation_ = std::move(continuation);

		a_.auth_.authenticate(username, {authentication::method::password{std::string(password)}}, req.get_peer_address_type(), req.get_peer_address(), *this);
	}

	void authenticate(const authentication::refresh_token &refresh_token, http::server::request &req, http::server::shared_transaction t, continuation_type continuation)
	{
		t_ = std::move(t);
		continuation_ = std::move(continuation);

		a_.auth_.authenticate(refresh_token.username, {authentication::method::token{refresh_token, a_.tm_}}, req.get_peer_address_type(), req.get_peer_address(), *this);
	}

	~worker() override
	{
		a_.auth_.stop_ongoing_authentications(*this);
		remove_handler();
	}

private:
	void on_auth_result(authentication::authenticator &, std::unique_ptr<authentication::authenticator::operation> &op)
	{
		auto session_user = authentication::session_user(std::move(op), a_.logger_);

		if (t_) {
			auto &res = t_->res();

			if (!session_user) {
				res.send_status(401, "Unauthorized") &&
				res.send_header(headers::WWW_Authenticate, "Bearer") &&
				res.send_end();

				t_ = nullptr;
				continuation_ = nullptr;
				return;
			}
		}

		continuation_(std::move(session_user));
		t_ = nullptr;
		continuation_ = nullptr;
	}

	void operator()(const event_base &ev) override
	{
		fz::dispatch<
			authentication::authenticator::operation::result_event
		>(ev, this,
			&worker::on_auth_result
		);
	}

	authorizator &a_;
	http::server::shared_transaction t_{};
	continuation_type continuation_{};
};

authorizator::authorizator(event_loop &loop, authentication::authenticator &auth, authentication::token_manager &tm, logger_interface &logger)
	: event_handler(loop)
	, key_(symmetric_key::generate())
	, auth_(auth)
	, tm_(tm)
	, logger_(logger, "Authorizator")
	, authorizations_p_(std::make_unique<std::unordered_map<std::size_t, authorization>>())
	, workers_p_(std::make_unique<std::unordered_map<event_loop *, worker>>())
	, authorizations_(*authorizations_p_)
	, workers_(*workers_p_)
{}

authorizator::~authorizator()
{
	remove_handler();
}

std::optional<std::string_view> authorizator::get_access_token_bearer(server::request &req)
{
	auto bearer = [](std::string_view authorization) -> field::component_view {
		std::string_view token;
		util::parseable_range r(authorization);

		std::size_t idx{};

		for (auto t: strtokenizer(authorization, " ", true)) {
			if (idx == 0) {
				if (t != "Bearer") {
					break;
				}
			}
			else
				if (idx == 1) {
					token = t;
				}
				else {
					token = {};
					break;
				}

			idx += 1;
		}

		return token;
	}(req.headers.get(headers::Authorization));

	if (bearer == "cookie:access_token") {
		if (auto cookie = req.headers.get_cookie("access_token", req.is_secure())) {
			bearer = cookie;
		}
		else {
			logger_.log(logmsg::debug_info, L"Bearer is set to cookie:access_token, but the cookie doesn't exist.");
		}
	}

	return bearer;
}

util::locked_proxy<authorizator::authorization> authorizator::get_authorization(std::string_view bearer)
{
	if (bearer.empty()) {
		return {};
	}

	auto access_token = authentication::access_token::decrypt(bearer, key_);
	if (!access_token) {
		return {};
	}

	return get_authorization(access_token);
}

util::locked_proxy<authorizator::authorization> authorizator::get_authorization(const authentication::access_token &access_token)
{
	mutex_.lock();

	auto it = authorizations_.find(access_token.id);
	if (it == authorizations_.end() || access_token != it->second.get_refresh_token().access) {
		mutex_.unlock();
		return {};
	}

	return {&it->second, &mutex_};
}

util::locked_proxy<authorizator::authorization> authorizator::make_authorization(authentication::session_user session_user, authentication::refresh_token refresh_token)
{
	if (!session_user) {
		return {};
	}

	if (!refresh_token) {
		refresh_token = tm_.create(session_user, refresh_token_timeout_);
	}
	else
	if (session_user->lock()->name != refresh_token.username) {
		logger_.log(logmsg::error, L"The passed in refresh_token doesn't belong to the user [%s]", session_user->lock()->name);
		return {};
	}

	if (!refresh_token) {
		logger_.log(logmsg::error, L"Couldn't create the refresh token for user %s. This is an internal error.", session_user->lock()->name);
		return {};
	}

	mutex_.lock();
	logger_.log(logmsg::debug_info, L"Authorization for user %s with id (%d, %d) created.", refresh_token.username, refresh_token.access.id, refresh_token.access.refresh_id);

	auto access_id = refresh_token.access.id;
	auto res = authorizations_.try_emplace(access_id, std::move(session_user), std::move(refresh_token), *this);

	if (!res.second) {
		logger_.log(logmsg::error, L"Couldn't store the authorization for user %s. This is an internal error.", refresh_token.username);
		tm_.destroy(refresh_token);

		mutex_.unlock();
		return {};
	}


	return {&res.first->second, &mutex_};
}

std::optional<authorizator::authorization_data<>> authorizator::get_authorization_data(const server::shared_transaction &t, custom_authorization_data_factory *adf)
{
	auto bearer = get_access_token_bearer(t->req());
	if (!bearer) {
		return std::nullopt;
	}

	auto ret = [&]() -> std::optional<authorizator::authorization_data<>> {
		auto authorization = get_authorization(*bearer);
		if (!authorization) {
			return std::nullopt;
		}

		return authorization->get_data(adf);
	}();

	if (!ret) {
		auto &res = t->res();

		res.send_status(401, "Unauthorized") &&
		res.send_header(headers::WWW_Authenticate, "Bearer");
		res.send_end();
	}

	return ret;
}

void authorizator::authorize(const authentication::refresh_token &refresh_token, event_loop &loop, server::request &req, custom_authorization_data_factory *adf, std::function<void (std::optional<authorization_data<>>)> continuation)
{
	auto ret = [&]() -> std::optional<authorizator::authorization_data<>> {
		auto authorization = get_authorization(refresh_token.access);
		if (!authorization) {
			return std::nullopt;
		}

		return authorization->get_data(adf);
	}();

	if (ret) {
		return continuation(std::move(ret));
	}

	workers_.try_emplace(&loop, *this, loop).first->second.authenticate(refresh_token, req, nullptr,
	[this, adf, refresh_token, continuation = std::move(continuation)](authentication::session_user session_user) mutable {
		if (auto authorization = make_authorization(std::move(session_user), std::move(refresh_token))) {
			continuation(authorization->get_data(adf));
		}
		else {
			return continuation(std::nullopt);
		}
	});
}

void authorizator::set_timeouts(duration access_token_timeout, duration refresh_token_timeout)
{
	scoped_lock lock(mutex_);

	access_token_timeout_ = access_token_timeout;
	refresh_token_timeout_ = refresh_token_timeout;
}

void authorizator::reset()
{
	scoped_lock lock(mutex_);

	logger_.log(logmsg::debug_info, L"Revoking all authorizations.");
	authorizations_.clear();
	tm_.reset();
}

void authorizator::handle_transaction(const server::shared_transaction &t)
{
	auto &req = t->req();
	auto &res = t->res();

	if (!req.is_secure()) {
		res.send_status(403, "Forbidden") &&
		res.send_body("This endpoint can be accessed only via HTTPS\n");
		return;
	}

	if (req.method != "POST") {
		res.send_status(405, "Method Not Allowed") &&
		res.send_header(fz::http::headers::Allowed, "POST") &&
		res.send_end();

		return;
	}

	auto const &content_type = req.headers.get(http::headers::Content_Type);
	bool is_urlencoded = content_type.is("application/x-www-form-urlencoded");

	if (!is_urlencoded) {
		res.send_status(415, "Unsupported Media Type") &&
		res.send_end();
		return;
	}

	bool is_token = false;

	// https://www.rfc-editor.org/rfc/rfc6749
	if (req.uri.path_ == "/token") {
		is_token = true;
	}
	else
	// https://www.rfc-editor.org/rfc/rfc7009
	if (req.uri.path_ == "/revoke") {
		is_token = false;
	}
	else {
		res.send_status(404, "Not Found") &&
		res.send_end();
		return;
	}

	req.receive_body(std::string(), [this, is_token, wt = std::weak_ptr(t)](std::string body, bool success) mutable {
		auto t = wt.lock();
		if (!t) {
			return;
		}

		if (!success) {
			auto &res = t->res();

			res.send_status(500, "Internal Server Error") &&
			res.send_header(http::headers::Connection, "close") &&
			res.send_end();

			return;
		}

		if (is_token) {
			return do_token(query_string(body), t);
		}

		return do_revoke(query_string(body), t);
	});
}


void authorizator::do_token(query_string q, const server::shared_transaction &t)
{
	auto &grant_type = q["grant_type"];

	if (grant_type == "password") {
		return do_token_password(q["username"], q["password"], q["cookie_path"], t);
	}

	if (grant_type == "refresh_token") {
		return do_token_refresh(q["refresh_token"], q["cookie_path"], t);
	}

	send_auth_error(t->res(), "unsupported_grant_type");
}

void authorizator::do_revoke(query_string q, const server::shared_transaction &t)
{
	std::string_view bearer = q["token"];
	std::string_view hint = q["token_type_hint"];

	auto &req = t->req();
	auto &res = t->res();

	if (bearer.empty()) {
		send_auth_error(res, "invalid_request", "token is absent");
		return;
	}

	bool has_refresh_cookie{};
	bool must_erase_refresh_cookies{};

	if (bearer == "cookie:access_token") {
		if (auto cookie = req.headers.get_cookie("access_token", req.is_secure())) {
			bearer = cookie;
		}
	}
	else
	if (bearer == "cookie:refresh_token") {
		if (auto cookie = req.headers.get_cookie("refresh_token", req.is_secure())) {
			has_refresh_cookie = true;
			bearer = cookie;
		}
	}

	auto access_token = [&] {
		if (auto refresh_token = authentication::refresh_token::decrypt(bearer, tm_.get_symmetric_key())) {
			scoped_lock lock(mutex_);
			if (tm_.destroy(refresh_token)) {
				logger_.log(logmsg::debug_info, L"Revoked refresh token with id (%d,%d).", refresh_token.access.id, refresh_token.access.refresh_id);
			}

			must_erase_refresh_cookies = has_refresh_cookie;

			return std::move(refresh_token.access);
		}

		return authentication::access_token::decrypt(bearer, key_);
	}();

	if (access_token) {
		scoped_lock lock(mutex_);
		if (authorizations_.erase(access_token.id)) {
			logger_.log(logmsg::debug_info, L"Revoked access token with id (%d,%d).", access_token.id, access_token.refresh_id);
		}
	}


	res.send_status(200, "Ok") && [&] {
		if (hint == "refresh_token" || hint.empty()) {
			if (must_erase_refresh_cookies) {
				auto revoke_token_path = req.headers.get(headers::X_FZ_INT_Original_Path, req.uri.path_);
				auto refresh_token_path = util::fs::absolute_unix_path(revoke_token_path).parent() / "token";

				res.send_headers({
					{headers::Set_Cookie, headers::make_cookie("refresh_token", "", refresh_token_path, req.is_secure(), true, duration::from_seconds(0))},
					{headers::Set_Cookie, headers::make_cookie("refresh_token", "", revoke_token_path, req.is_secure(), true, duration::from_seconds(0))},
				});
			}
		}

		return true;
	}() &&
	res.send_end();
}

void authorizator::do_token_password(std::string username, std::string password, std::string cookie_path, const server::shared_transaction &t)
{
	auto &req = t->req();
	auto &res = t->res();

	if (username.empty()) {
		return send_auth_error(res, "invalid_request", "username empty or absent");
	}

	workers_.try_emplace(&t->get_event_loop(), *this, t->get_event_loop()).first->second.authenticate(std::move(username), std::move(password), req, t,
	[=](authentication::session_user session_user) {
		auto authorization = make_authorization(std::move(session_user));
		send_auth_tokens(std::move(authorization), std::move(cookie_path), t);
	});
}

void authorizator::do_token_refresh(std::string bearer, std::string cookie_path, const server::shared_transaction &t)
{
	auto &req = t->req();

	if (bearer == "cookie:refresh_token") {
		if (auto cookie = req.headers.get_cookie("refresh_token", req.is_secure())) {
			bearer = cookie;
		}
		else {
			logger_.log(logmsg::debug_info, L"Bearer is set to cookie:refresh_token, but the cookie doesn't exist.");
		}
	}

	auto refresh_token = authentication::refresh_token::decrypt(bearer, tm_.get_symmetric_key());
	if (!refresh_token) {
		return send_auth_error(t->res(), "invalid_request", "refresh token corrupted or absent");
	}

	workers_.try_emplace(&t->get_event_loop(), *this, t->get_event_loop()).first->second.authenticate(refresh_token, req, t,
	[this, refresh_token, cookie_path=std::move(cookie_path), t](authentication::session_user session_user) {
		auto authorization = get_authorization(refresh_token.access);
		if (authorization) {
			authorization->set_session_user(std::move(session_user));
		}
		else {
			authorization = make_authorization(std::move(session_user), tm_.refresh(refresh_token));
		}

		send_auth_tokens(std::move(authorization), std::move(cookie_path), t);
	});
}

void authorizator::send_auth_tokens(util::locked_proxy<authorization> authorization, std::string cookie_path, const server::shared_transaction &t)
{
	auto [refresh_token, access_timeout, refresh_timeout] = [this, authorization = std::move(authorization)]() -> std::tuple<const authentication::refresh_token &, duration, duration>  {
		if (authorization) {
			return { authorization->get_refresh_token(), access_token_timeout_, refresh_token_timeout_ };
		}

		static const authentication::refresh_token null_refresh_token;
		return { null_refresh_token, {}, {} };
	}();

	auto refresh_bearer = refresh_token.encrypt(tm_.get_symmetric_key());
	auto access_bearer = refresh_token.access.encrypt(key_);

	auto &req = t->req();
	auto &res = t->res();

	if (!refresh_token || refresh_bearer.empty() || access_bearer.empty()) {
		res.send_status(500, "Internal Server Error") &&
		res.send_header(headers::Connection, "close") &&
		res.send_end();

		return;
	}

	res.send_status(200, "Ok");
	res.send_headers({
		{headers::Content_Type, "application/json"},
		{headers::Cache_Control, "no-store"},
		{headers::Pragma, "no-cache"}
	});

	json j;

	j["token_type"] = "bearer";
	j["expires_in"] = access_timeout.get_seconds();

	if (cookie_path.empty()) {
		j["access_token"] = std::move(access_bearer);
		j["refresh_token"] = std::move(refresh_bearer);
	}
	else {
		j["access_token"] = "cookie:access_token";
		j["refresh_token"] = "cookie:refresh_token";

		auto refresh_token_path = req.headers.get(headers::X_FZ_INT_Original_Path, req.uri.path_);
		auto revoke_token_path = util::fs::absolute_unix_path(refresh_token_path).parent() / "revoke";

		res.send_headers({
			{headers::Set_Cookie, headers::make_cookie("access_token", access_bearer, cookie_path, req.is_secure(), true, access_timeout)},
			{headers::Set_Cookie, headers::make_cookie("refresh_token", refresh_bearer, refresh_token_path, req.is_secure(), true, refresh_timeout)},
			{headers::Set_Cookie, headers::make_cookie("access_token", access_bearer, revoke_token_path, req.is_secure(), true, access_timeout)},
			{headers::Set_Cookie, headers::make_cookie("refresh_token", refresh_bearer, revoke_token_path, req.is_secure(), true, refresh_timeout)},
		});
	}

	res.send_body(j.to_string());
}


void authorizator::operator()(const event_base &ev)
{
	fz::dispatch<authorization::expired_event>(ev, [&](authorization &a) {
		scoped_lock lock(mutex_);

		auto access_token = a.get_refresh_token().access;

		logger_.log_u(logmsg::debug_info, L"Erasing authorization with id (%d, %d).", access_token.id, access_token.refresh_id);
		authorizations_.erase(access_token.id);
	});
}

void authorizator::send_auth_error( server::responder &res, std::string_view error, std::string_view description)
{
	res.send_status(400, "Bad Request") && res.send_header(headers::Content_Type, "application/json") && [&] {
		if (description.empty()) {
			return res.send_body(fz::sprintf(R"({"error":"%s"})", error));
		}

		return res.send_body(fz::sprintf(R"({"error":"%s","description":"%s"})", error, description));
	}();
}

authorizator::custom_authorization_data_factory::~custom_authorization_data_factory()
{}

}
