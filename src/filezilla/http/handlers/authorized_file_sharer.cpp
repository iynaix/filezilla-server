#include "authorized_file_sharer.hpp"
#include "../server/responder.hpp"

#include "../../authentication/password.hpp"
#include "../../serialization/archives/binary.hpp"
#include "../../serialization/types/optional.hpp"

namespace fz::http::handlers
{

namespace {

struct share_token
{
	authentication::refresh_token refresh;
	std::optional<authentication::password::pbkdf2::hmac_sha256> password;

	operator bool() const
	{
		return refresh;
	}

	template <typename Archive>
	void serialize(Archive &ar)
	{
		ar(refresh, password);
	}

	std::string encrypt(const symmetric_key &key) const
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

	static share_token decrypt(std::string_view encrypted, const symmetric_key &key)
	{
		auto decoded = base64_decode(encrypted);

		if (!decoded.empty()) {
			auto plain = fz::decrypt(decoded, key);

			if (!plain.empty()) {
				auto token = serialization::binary_input_archive::decode<share_token>(plain);

				if (token) {
					return std::move(*token);
				}
			}
		}

		return {};
	}
};

}

struct authorized_file_sharer::custom_authorization_data
{
	custom_authorization_data(logger_interface &logger, file_server::options opts = {})
		: tvfs(logger)
		, fs(tvfs, logger, std::move(opts))
	{}

	tvfs::engine tvfs;
	file_server fs;

	static std::shared_ptr<custom_authorization_data> get(std::optional<authorizator::authorization_data<>> d, const util::fs::absolute_unix_path &path)
	{
		if (d) {
			auto unc = notifications_count(d->user);
			auto c = std::static_pointer_cast<custom_authorization_data>(d->custom);

			if (unc != c->unc_) {
				c->unc_ = unc;

				auto u = d->user->lock();

				auto mt = std::make_shared<tvfs::mount_tree>(*u->mount_tree);
				if (!mt->set_root(path)) {
					mt.reset();
				}

				c->tvfs.set_mount_tree(std::move(mt));
				c->tvfs.set_backend(u->impersonator);
				c->tvfs.set_open_limits(u->session_open_limits);
			}

			return c;
		}

		return {};
	}


	static std::optional<authorizator::authorization_data<custom_authorization_data>> get(const server::shared_transaction &t, authorized_file_sharer &fs)
	{
		if (auto a = fs.auth_.get_authorization_data(t, &fs)) {
			auto unc = notifications_count(a->user);
			auto d = std::move(a)->as<custom_authorization_data>();
			auto &c = d->custom;

			if (unc != c->unc_) {
				c->unc_ = unc;

				auto u = d->user->lock();

				c->tvfs.set_mount_tree(u->mount_tree);
				c->tvfs.set_backend(u->impersonator);
				c->tvfs.set_open_limits(u->session_open_limits);
			}

			return d;
		}

		return {};
	}

private:
	std::size_t unc_{std::size_t(-1)};
};

authorized_file_sharer::authorized_file_sharer(authorizator &auth, logger_interface &logger, file_server::options opts)
	: auth_(auth)
	, logger_(logger)
	, opts_(std::move(opts))
{
}

std::shared_ptr<void> authorized_file_sharer::make_custom_authorization_data()
{
	return std::make_shared<custom_authorization_data>(logger_, opts_);
}

void authorized_file_sharer::handle_transaction(const server::shared_transaction &t)
{
	auto &req = t->req();
	auto &res = t->res();

	if (req.uri.path_ == "/") {
		return do_create(t);
	}

	auto bearer = std::string_view(req.uri.path_).substr(1);
	auto slash_pos = bearer.find('/');
	bearer = bearer.substr(0, slash_pos);

	if (slash_pos == std::string::npos) {
		auto location = percent_encode(req.headers.get(headers::X_FZ_INT_Original_Path, req.uri.path_), true) + '/';
		if (!req.uri.query_.empty()) {
			location += '?';
			location += req.uri.query_;
		}

		// If there's no slash at the end of the bearer, redirect to the same url, with the slash appended.
		res.send_status(301, "Moved Permanently") &&
		res.send_header(http::headers::Location, location) &&
		res.send_end();

		return;
	}

	auto share_token = share_token::decrypt(bearer, auth_.get_token_manager().get_symmetric_key());
	if (!share_token) {
		res.send_status(404, "Not Found") &&
		res.send_end();

		return;
	}

	if (share_token.password) {
		auto clear_password = [&]() -> std::string {
			auto authorization = req.headers.get(headers::Authorization);
			if (!authorization) {
				// We allow the authorization to be in the query parameter,
				// mainly for the webui to use when creating a download link.
				// It's in general not secure to transmit the password in the URL,
				// because URLs are usually logged and end up in caches, but
				// in our case this is safe because the password is randomly generated and used for a very short time.

				query_string q(req.uri.query_);
				return base64_decode_s(q["authorization"]);
			}

			auto res = strtok_view(authorization, " ");
			if (res.size() != 2 || res[0] != "Basic") {
				return {};
			}

			auto user_pass = base64_decode_s(res[1]);

			res = strtok_view(user_pass, ":", false);
			if (res.size() != 2) {
				return {};
			}

			return std::string(res[1]);
		}();

		if (clear_password.empty() || !share_token.password->verify(clear_password)) {
			res.send_status(401, "Unauthorized") &&
			res.send_header(headers::WWW_Authenticate, fz::sprintf(R"(Basic realm="Password needed for %s")", bearer)) &&
			res.send_end();

			return;
		}
	}

	req.uri.path_ = req.uri.path_.substr(bearer.size()+1);

	auth_.authorize(share_token.refresh, t->get_event_loop(), req, this,
	[this, wt = std::weak_ptr(t), path = util::fs::absolute_unix_path(share_token.refresh.path)](std::optional<authorizator::authorization_data<>> d) mutable {
		auto t = wt.lock();
		if (!t) {
			logger_.log_raw(logmsg::error, L"Couldn't lock the weak ptr to the transaction. This is an unexpected internal error.");
			return;
		}

		auto &req = t->req();
		auto &res = t->res();

		if (auto c = custom_authorization_data::get(std::move(d), path)) {
			if (req.uri.path_ == "/") {
				req.headers[headers::X_FZ_INT_File_Name] = path.base();
			}

			c->fs.handle_transaction(t);
		}
		else {
			res.send_status(403, "Forbidden") &&
			res.send_end();
		}
	});
}

void authorized_file_sharer::do_create(const server::shared_transaction &t)
{
	auto &req = t->req();
	auto &res = t->res();

	if (req.method != "POST") {
		res.send_status(405, "Method Not Allowed") &&
		res.send_header(fz::http::headers::Allowed, "POST") &&
		res.send_end();

		return;
	}

	auto d = custom_authorization_data::get(t, *this);
	if (!d) {
		return;
	}

	auto const &content_type = req.headers.get(http::headers::Content_Type);
	bool is_urlencoded = content_type.is("application/x-www-form-urlencoded");

	if (!is_urlencoded) {
		res.send_status(415, "Unsupported Media Type") &&
		res.send_end();
		return;
	}

	req.receive_body(std::string(), [this, d = std::move(d), &res](std::string body, bool success) {
		if (!success) {
			res.send_status(500, "Internal Server Error") &&
			res.send_header(http::headers::Connection, "close") &&
			res.send_end();
			return;
		}

		query_string q(body);
		util::fs::absolute_unix_path path = q["path"];
		std::string_view password_string = q["password"];
		std::string_view expires_in_string = q["expires_in"];

		if (!path) {
			res.send_status(400, "Bad Request") &&
			res.send_body("Invalid or missing path.");
			return;
		}

		auto secs = fz::to_integral<std::int64_t>(expires_in_string, -1);
		if (secs < 0) {
			if (expires_in_string.empty()) {
				secs = 0;
			}
			else {
				res.send_status(400, "Bad Request") &&
				res.send_body("Invalid expiration.");
				return;
			}
		}

		auto expires_in = duration::from_seconds(secs);

		auto password = [&]() -> std::optional<authentication::password::pbkdf2::hmac_sha256> {
			if (!password_string.empty()) {
				return {password_string};
			}

			return std::nullopt;
		}();

		// Let's check whether the path is reachable at all
		if (d->custom->fs.get_file_type_or_send_error(path, res) == local_filesys::unknown) {
			return;
		}

		// If it is, then let's generate the token
		auto refresh_token = auth_.get_token_manager().create(d->user, expires_in, path);
		share_token st = { std::move(refresh_token), std::move(password) };
		auto encrypted = st.encrypt(auth_.get_token_manager().get_symmetric_key());

		if (!st || encrypted.empty()) {
			res.send_status(500, "Internal Server Error") &&
			res.send_header(http::headers::Connection, "close") &&
			res.send_end();

			return;
		}

		res.send_status(200, "Ok") &&
		res.send_header(http::headers::Content_Type, "application/json") &&
		res.send_body(fz::sprintf(R"({"share_token":"%s"})", encrypted));
	});
}

}
