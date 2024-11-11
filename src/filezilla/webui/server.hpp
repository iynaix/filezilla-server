#ifndef FZ_WEBUI_SERVER_HPP
#define FZ_WEBUI_SERVER_HPP

#include <libfilezilla/time.hpp>
#include "../authentication/authenticator.hpp"
#include "../authentication/sqlite_token_db.hpp"
#include "../tvfs/engine.hpp"
#include "../http/server.hpp"
#include "../http/handlers/authorizator.hpp"
#include "../http/handlers/authorized_file_server.hpp"
#include "../http/handlers/authorized_file_sharer.hpp"
#include "../http/handlers/router.hpp"
#include "templated_index_wrapper.hpp"
#include "rewriter.hpp"

#ifdef HAVE_CONFIG_H
#   include "config_modules.hpp"
#endif

namespace fz::webui {

class server
{
public:
	struct options
	{
		duration access_token_timeout = duration::from_seconds(300);
		duration refresh_token_timeout = duration::from_days(15);
		duration http_keepalive_timeout = duration::from_seconds(15);
		duration http_activity_timeout = duration::from_seconds(30);

		std::vector<http::server::address_info> listeners_info {};
		securable_socket::info tls{};


		options(){}
	};

#if ENABLE_FZ_WEBUI

	server(tcp::server::context &context, event_loop_pool &event_loop_pool, const util::fs::absolute_native_path &app_root, const util::fs::absolute_native_path &tokendb_file,
		   tcp::address_list &disallowed_ips, tcp::address_list &allowed_ips, authentication::autobanner &autobanner, authentication::authenticator &auth,
		   logger_interface &logger, options opts = {});

	void set_options(options opts);
	options get_options() const;

	bool start();
	bool stop(bool destroy_all_sessions);

	void reset_tokens();

private:
	mutable mutex mutex_;

	logger::modularized logger_;
	options opts_;
	tvfs::engine app_tvfs_;
	http::handlers::file_server app_file_server_;
	authentication::in_memory_token_db imtdb_;
	authentication::sqlite_token_db tdb_;
	authentication::token_manager tm_;
	http::handlers::authorizator authorizator_;
	http::handlers::authorized_file_server user_file_server_;
	http::handlers::authorized_file_sharer file_sharer_;
	webui::templated_index_wrapper templated_index_wrapper_;
	http::handlers::router router_;
	webui::rewriter rewriter_;
	http::server http_;

#endif
};

}

#endif // FZ_WEBUI_SERVER_HPP
