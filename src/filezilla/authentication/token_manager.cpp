#include <libfilezilla/json.hpp>
#include <libfilezilla/encode.hpp>

#include "../logger/type.hpp"
#include "../serialization/archives/binary.hpp"
#include "../serialization/types/containers.hpp"

#include "token_manager.hpp"


namespace fz::authentication
{

token_manager::~token_manager()
{}

token_manager::token_manager(token_db &db, logger_interface &logger)
	: db_(db)
	, logger_(logger, "Token Manager")
{
}

bool token_manager::verify(std::string_view username, const refresh_token &token, impersonation_token &impersonation)
{
	if (username != token.username) {
		logger_.log(logmsg::error, L"[verify] [user: %s] Given username does not match with the token username [%s].", username, token.username);
		return false;
	}

	auto our = db_.select(token.access.id);
	if (!our) {
		logger_.log(logmsg::error, L"[verify] [user: %s] Could not find the token in the database.", username);
		return false;
	}

	if (username != our.refresh.username) {
		logger_.log(logmsg::error, L"[verify] [user: %s] The token in the database is associated with a different user name: %s.", username, our.refresh.username);
		return false;
	}

	if (token.access.refresh_id != our.refresh.access.refresh_id) {
		logger_.log(logmsg::error, L"[verify] [user: %s] Attempted to verify an invalid token. As a protection measure, the current token is being invalidated too.", username);
		db_.remove(our.refresh.access.refresh_id);
		return false;
	}

	if (our.expires_at && our.expires_at <= datetime::now()) {
		logger_.log(logmsg::error, L"[verify] [user: %s] The token has expired on %s, invalidating it.", username, our.expires_at.get_rfc822());
		db_.remove(our.refresh.access.refresh_id);
		return false;
	}

	if (our.must_impersonate) {
	#ifdef FZ_WINDOWS
		logger_.log(logmsg::error, L"[verify] [user: %s] We do not yet support token impersonation under Windows.", username);
		return false;
	#else
		impersonation_token imp(fz::to_native(username), impersonation_flag::pwless, {});
		if (!imp) {
			logger_.log(logmsg::error, L"[verify] [user: %s] Could not impersonate the user.", username);
			return false;
		}

		impersonation = std::move(imp);
	#endif
	}

	logger_.log(logmsg::debug_info, L"[verify] [user: %s] Token successfully validated.", username);

	return true;
}

refresh_token token_manager::create(const shared_user &su, duration expiration, std::string_view path)
{
	if (!su) {
		logger_.log_u(logmsg::error, L"[create] The passed in shared_user is null.");
		return {};
	}

	std::string name;
	bool needs_impersonation;

	if (auto u = su->lock()) {
		if (!path.empty() && !util::fs::absolute_unix_path(path)) {
			logger_.log_u(logmsg::error, L"[create] [user: %s] The passed in path is not a valid absolute unix-like path.");
			return {};
		}

		needs_impersonation = u->get_impersonation_token() && fz::to_utf8(u->get_impersonation_token().username()) == u->name;
		name = u->name;

		#ifdef FZ_WINDOWS
			if (needs_impersonation) {
				logger_.log(logmsg::error, L"[create] [user: %s] We do not yet support token impersonation under Windows.", u->name);
				return {};
			}
		#endif
	}
	else {
		logger_.log_u(logmsg::error, L"[create] The passed in shared_user is not valid.");
		return {};
	}

	scoped_lock lock(mutex_);
	auto token = db_.insert(name, std::string(path), needs_impersonation, expiration);
	if (!token) {
		logger_.log_u(logmsg::error, L"[create] [user: %s] Couldn't add the token to the DB.", name);
		return {};
	}

	return token.refresh;
}

refresh_token token_manager::refresh(const refresh_token &old)
{
	scoped_lock lock(mutex_);

	auto our = db_.select(old.access.id);
	if (!our) {
		logger_.log(logmsg::error, L"[refresh] [user: %s] Could not find the token in the database.", old.username);
		return {};
	}

	if (old.username != our.refresh.username) {
		logger_.log(logmsg::error, L"[refresh] [user: %s] The token in the database is associated with a different user name: %s.", old.username, our.refresh.username);
		return {};
	}

	if (old.access.refresh_id != our.refresh.access.refresh_id) {
		logger_.log(logmsg::error, L"[refresh] [user: %s] Attempted to refresh an invalid token.", old.username);
		return {};
	}

	our.refresh.access.refresh_id += 1;

	if (db_.update(our)) {
		return our.refresh;
	}

	return {};
}

bool token_manager::destroy(const refresh_token &token)
{
	scoped_lock lock(mutex_);

	auto our = db_.select(token.access.id);
	if (!our) {
		logger_.log(logmsg::warning, L"[destroy] [user: %s] Could not find the token in the database.", token.username);
		return false;
	}

	if (token.username != our.refresh.username) {
		logger_.log(logmsg::warning, L"[destroy] [user: %s] The token in the database is associated with a different user name: %s.", token.username, our.refresh.username);
		return false;
	}

	return db_.remove(token.access.id);
}

void token_manager::reset()
{
	db_.reset();
}

const symmetric_key &token_manager::get_symmetric_key()
{
	return db_.get_symmetric_key();
}

template <typename Archive>
void access_token::serialize(Archive &ar)
{
	ar(id, refresh_id);
}

template <typename Archive>
void refresh_token::serialize(Archive &ar)
{
	ar(access, username, path);
}

std::string access_token::encrypt(const symmetric_key &key) const
{
	auto plain = serialization::binary_output_archive::encode(*this);

	if (!plain.empty()) {
		auto encrypted = fz::encrypt(plain.to_view(), key);

		if (!encrypted.empty())	{
			return base64_encode(encrypted, base64_type::url, false);
		}
	}

	return {};
}

access_token access_token::decrypt(std::string_view encrypted, const symmetric_key &key)
{
	auto decoded = base64_decode(encrypted);

	if (!decoded.empty()) {
		auto plain = fz::decrypt(decoded, key);

		if (!plain.empty()) {
			auto token = serialization::binary_input_archive::decode<access_token>(plain);

			if (token) {
				return std::move(*token);
			}
		}
	}

	return {};
}

std::string refresh_token::encrypt(const symmetric_key &key) const
{
	auto plain = serialization::binary_output_archive::encode(*this);

	if (!plain.empty()) {
		auto encrypted = fz::encrypt(plain.to_view(), key);

		if (!encrypted.empty())	{
			return base64_encode(encrypted, base64_type::url, false);
		}
	}

	return {};
}

refresh_token refresh_token::decrypt(std::string_view encrypted, const symmetric_key &key)
{
	auto decoded = base64_decode(encrypted);

	if (!decoded.empty()) {
		auto plain = fz::decrypt(decoded, key);

		if (!plain.empty()) {
			auto token = serialization::binary_input_archive::decode<refresh_token>(plain);

			if (token) {
				return std::move(*token);
			}
		}
	}

	return {};
}

token_db::~token_db()
{

}

template void access_token::serialize<serialization::binary_input_archive>(serialization::binary_input_archive &);
template void access_token::serialize<serialization::binary_output_archive>(serialization::binary_output_archive &);

template void refresh_token::serialize<serialization::binary_input_archive>(serialization::binary_input_archive &);
template void refresh_token::serialize<serialization::binary_output_archive>(serialization::binary_output_archive &);

}
