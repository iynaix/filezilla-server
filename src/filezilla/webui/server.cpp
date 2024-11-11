#include "server.hpp"

#include "../tvfs/engine.hpp"

namespace fz::webui
{

server::server(tcp::server::context &context, event_loop_pool &event_loop_pool, const util::fs::absolute_native_path &app_root, const util::fs::absolute_native_path &tokendb_file, tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, authentication::authenticator &auth, logger_interface &logger, options opts)
	: logger_(logger, "WebUI")
	, app_tvfs_(logger_)
	, app_file_server_(app_tvfs_, logger_, http::handlers::file_server::options().default_index({"index.html"}))
	, tdb_(tokendb_file, logger_)
	, tm_(tokendb_file.str().empty() ? (authentication::token_db&)imtdb_ : tdb_, logger_)
	, authorizator_(context.loop(), auth, tm_, logger_)
	, user_file_server_(authorizator_, logger_, http::handlers::file_server::options()
		.can_list_dir(true)
		.can_delete(true)
		.can_get(true)
		.can_put(true)
		.can_post(true)
		.honor_406(true))
	, file_sharer_(authorizator_, logger_, http::handlers::file_server::options()
		.can_list_dir(true)
		.can_delete(true)
		.can_get(true)
		.can_put(true)
		.can_post(true)
		.honor_406(true))
	, templated_index_wrapper_(app_file_server_)
	, rewriter_(router_)
	, http_(context, event_loop_pool, rewriter_, disallowed_ips, allowed_ips, autobanner, logger_)
{
	if (!app_root) {
		logger_.log(logmsg::error, L"app_root is not set or is invalid, this means that the WebUI will not be accessible, but the REST api will still be functional.");
	}
	else {
		logger_.log_u(logmsg::debug_info, L"app_root set to: %s", app_root.str());
		if ((app_root / fzT("index.html")).type() != local_filesys::file) {
			logger_.log(logmsg::error, L"Couldn't find index.html in the app_root (%s).\nThis means that the WebUI will not be accessible, but the REST api will still be functional.", app_root.str());
		}
	}

	app_tvfs_.set_mount_tree(std::make_shared<tvfs::mount_tree>(tvfs::mount_table({
		{ "/api", {}, fz::tvfs::mount_point::disabled, fz::tvfs::mount_point::do_not_apply_permissions_recursively },
		{ "/", app_root, fz::tvfs::mount_point::read_only, fz::tvfs::mount_point::apply_permissions_recursively }
	}), tvfs::placeholders::map{}, logger_));

	router_.add_route("/", templated_index_wrapper_);
	router_.add_route("/api/v1/auth", authorizator_);
	router_.add_route("/api/v1/files/home", user_file_server_);
	router_.add_route("/api/v1/files/shares", file_sharer_);

	set_options(std::move(opts));
}

void server::set_options(options opts)
{
	authorizator_.set_timeouts(opts.access_token_timeout, opts.refresh_token_timeout);
	http_.set_timeouts(opts.http_keepalive_timeout, opts.http_activity_timeout);
	http_.set_security_info(opts.tls);
	http_.set_listen_address_infos(opts.listeners_info);

	scoped_lock lock(mutex_);
	opts_ = std::move(opts);
}

server::options server::get_options() const
{
	scoped_lock lock(mutex_);
	return opts_;
}

bool server::start()
{
	return http_.start();
}

bool server::stop(bool destroy_all_sessions)
{
	return http_.stop(destroy_all_sessions);
}

void server::reset_tokens()
{
	authorizator_.reset();
}

}
