#include <iostream>

#include <libfilezilla/event_loop.hpp>
#include <libfilezilla/thread_pool.hpp>

#include "../../src/filezilla/logger/stdio.hpp"
#include "../../src/filezilla/http/server.hpp"
#include "../../src/filezilla/http/server/request.hpp"
#include "../../src/filezilla/http/server/responder.hpp"
#include "../../src/filezilla/serialization/archives/argv.hpp"
#include "../../src/filezilla/build_info.hpp"

#include "../../src/filezilla/tcp/binary_address_list.hpp"
#include "../../src/filezilla/authentication/autobanner.hpp"

#include "../../src/filezilla/tvfs/engine.hpp"
#include "../../src/filezilla/strresult.hpp"

#include "../../src/filezilla/util/options.hpp"
#include "../../src/filezilla/logger/type.hpp"
#include "../../src/filezilla/string.hpp"

#include "../../src/filezilla/http/handlers/file_server.hpp"
#include "../../src/filezilla/http/handlers/router.hpp"
#include "../../src/filezilla/http/handlers/authorizator.hpp"
#include "../../src/filezilla/http/handlers/authorized_file_server.hpp"
#include "../../src/filezilla/http/handlers/authorized_file_sharer.hpp"

#include "../../src/filezilla/webui/rewriter.hpp"
#include "../../src/filezilla/webui/templated_index_wrapper.hpp"

#include "../../src/filezilla/authentication/file_based_authenticator.hpp"

#include "../../src/filezilla/util/filesystem.hpp"
#include "../../src/filezilla/authentication/token_manager.hpp"

#include "../../src/filezilla/util/tools.hpp"
#include "../../src/filezilla/webui/server.hpp"

#include <iostream>

using namespace fz;

int main(int argc, char *argv[])
{
	using namespace fz::serialization;

	bool print_help = false;
	bool verbose = false;
	fz::native_string webroot;
	std::string ip;
	std::uint16_t port = 0;
	bool use_tls = false;
	unsigned char num_threads = 0;

	std::string user;
	std::string password;
	util::fs::native_path userroot;
	util::fs::native_path userfile;

	setlocale(LC_ALL, "");

	{
		argv_input_archive ar{argc, argv};

		ar(optional_nvp{print_help, "help"});

		if (ar && !print_help) {
			ar(
				optional_nvp{print_help, "help"},
				optional_nvp{verbose, "verbose"},
				optional_nvp{num_threads, "threads"},
				optional_nvp{use_tls, "tls"},
				nvp{ip, "ip"},
				nvp{port, "port"},
				nvp{webroot, "dir"},
				optional_nvp(user, "user"),
				optional_nvp(password, "password"),
				optional_nvp(userroot, "userdir"),
				optional_nvp(userfile, "userfile")
			).check_for_unhandled_options();
		}

		if (!ar) {
			std::cerr << ar.error().description() << "\n";
			print_help = true;
		}

		if (userroot && userfile) {
			std::cerr << "Only one of --userdir and --userfile is allowed." << "\n";
			print_help = true;
		}

		if (print_help) {
			std::cerr
				<< fz::sprintf("httpserve v%s. Built for the %s flavour, on %s.", fz::build_info::version, fz::build_info::flavour, fz::build_info::datetime.get_rfc822()) << "\n"
				<< "Usage: " << argv[0] << " [--help] [--verbose] [--tls] [--threads <num threads>] [--user <username>] [--password <user password> [--userdir <user home dir> | --userfile <single user file>] --ip <ip> --port <port> --dir <webroot>"
				<< std::endl;

			return EXIT_FAILURE;
		}
	}

	event_loop loop {fz::event_loop::threadless};
	thread_pool pool;
	event_loop_pool loop_pool(loop, pool, num_threads);
	rate_limit_manager rlm(loop);
	logger::stdio logger {stderr};
	authentication::file_based_authenticator file_auth(pool, loop, logger, rlm);

	authentication::file_based_authenticator::users users;

	if (!user.empty()) {
		authentication::file_based_authenticator::user_entry u;
		if (!password.empty()) {
			u.credentials.password = authentication::password::pbkdf2::hmac_sha256(password);
			u.methods = u.credentials.get_most_secure_methods();
			u.methods.push_back(authentication::methods_set());
			u.methods.back().add<authentication::method::token>();
		}

		if (userroot) {
			u.mount_table = {
				{ "/", userroot, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification },
			};
		}

		if (userfile) {
			auto root = util::fs::unix_path("/");
			if (auto base = userfile.base()) {
				root /= fz::to_utf8(base);
			}

			u.mount_table = {
				{ root, userfile, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification },
			};
		}

		users.emplace(user, std::move(u));
	}

	file_auth.set_groups_and_users({}, std::move(users));

	if (verbose)
		logger.set_all(fz::logmsg::type(~0));

	tcp::server::context context(pool, loop);
	tcp::binary_address_list disallowed_ips;
	tcp::binary_address_list allowed_ips;
	authentication::autobanner autobanner(loop);

	auto tls = securable_socket::cert_info::generate_selfsigned({}, util::get_own_executable_directory(), logger);

#if 1
	webui::server::options opts;
	opts.listeners_info = {{{ip, port}, use_tls}};
	opts.tls = { tls, tls_ver::v1_2 };

	webui::server webui(context, loop_pool, webroot, fzT("/tmp/tokens.db"), disallowed_ips, allowed_ips, autobanner, file_auth, logger, std::move(opts));
	webui.start();

#else
	tvfs::engine tvfs(logger);
	tvfs.set_mount_tree(std::make_shared<tvfs::mount_tree>(tvfs::mount_table({
		{ "/api", {}, fz::tvfs::mount_point::disabled, fz::tvfs::mount_point::do_not_apply_permissions_recursively },
		{ "/", webroot, fz::tvfs::mount_point::read_only, fz::tvfs::mount_point::apply_permissions_recursively }
	}), tvfs::placeholders::map{}, logger));

	http::handlers::file_server file_server(tvfs, logger, http::handlers::file_server::options().can_list_dir(true).default_index({"index.html"}));
	authentication::in_memory_token_manager imtm(logger);
	http::handlers::authorizator authorizator(loop, file_auth, imtm, logger, duration::from_seconds(30));
	http::handlers::authorized_file_server authorized_file_server(authorizator, logger, http::handlers::file_server::options()
		.can_list_dir(true)
		.can_delete(true)
		.can_get(true)
		.can_put(true)
		.can_post(true)
		.honor_406(true)
	);

	http::handlers::authorized_file_sharer authorized_file_sharer(authorizator, logger, http::handlers::file_server::options()
		.can_list_dir(true)
		.can_delete(true)
		.can_get(true)
		.can_put(true)
		.can_post(true)
		.honor_406(true)
	);

	webui::templated_index_wrapper templated_index_wrapper(file_server);

	http::handlers::router router;

	router.add_route("/", templated_index_wrapper);
	router.add_route("/api/v1/auth", authorizator);
	router.add_route("/api/v1/files/home", authorized_file_server);
	router.add_route("/api/v1/files/shares", authorized_file_sharer);

	webui::rewriter rewriter{router};

	http::server s(context, loop_pool, rewriter, disallowed_ips, allowed_ips, autobanner, logger);


	s.set_security_info( { tls, tls_ver::v1_2 });
	s.set_listen_address_infos({{{ip, port}, use_tls}});
	s.set_timeouts(duration::from_seconds(600), duration::from_seconds(600));
	s.start();
#endif

	loop.run();

	return 0;
}
