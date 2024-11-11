#include <iostream>


#include <libfilezilla/event_loop.hpp>
#include <libfilezilla/thread_pool.hpp>

#include "../../src/filezilla/logger/stdio.hpp"
#include "../../src/filezilla/http/client.hpp"
#include "../../src/filezilla/serialization/archives/argv.hpp"
#include "../../src/filezilla/serialization/types/time.hpp"
#include "../../src/filezilla/build_info.hpp"

using namespace fz;


#if 0
c.perform("GET", fz::uri(tokens[2])).and_then([f = std::shared_ptr<fz::file>()](http::response::status reason, http::response &r) mutable {
	int err = 0;

	if (reason == http::response::got_headers)
		f = std::make_shared<fz::file>(fzT("/tmp/ciaociao"), fz::file::writing, fz::file::empty);
	else
	if (reason == http::response::got_body) {
		fz::util::io::write(*f, r.body, &err);
		r.body.clear();
	}
	else
	if (reason == http::response::got_end)
		f->close();

	return err;
});
#endif


int main(int argc, char *argv[])
{
	using namespace fz::serialization;

	bool insecure = false;
	bool print_help = false;
	bool verbose = false;
	duration timeout;
	std::string url;

	{
		argv_input_archive ar{argc, argv};

		ar(
			optional_nvp{insecure, "insecure"},
			optional_nvp{timeout, "timeout"},
			optional_nvp{print_help, "help"},
			optional_nvp{verbose, "verbose"},
			nvp{url, "fz:anonymous"}
		).check_for_unhandled_options();

		if (!ar) {
			std::cerr << ar.error().description() << "\n";
			print_help = true;
		}

		if (print_help) {
			std::cerr
				<< fz::sprintf("httpget v%s. Built for the %s flavour, on %s.", fz::build_info::version, fz::build_info::flavour, fz::build_info::datetime.get_rfc822()) << "\n"
				<< "Usage: " << argv[0] << " [--help] [--insecure] [--timeout=<timeout in milliseconds>] [--verbose] <url to get>"
				<< std::endl;

			return EXIT_FAILURE;
		}
	}

	event_loop loop {fz::event_loop::threadless};
	thread_pool pool;
	logger::stdio logger {stderr};
	fz::tls_system_trust_store trust_store(pool);

	if (verbose)
		logger.set_all(fz::logmsg::type(~0));

	http::client c(pool, loop, logger, http::client::options()
		.follow_redirects(true)
		.trust_store(&trust_store)
		.cert_verifier([&](const fz::tls_session_info &info) {
			if (!insecure && !info.system_trust()) {
				logger.log_u(logmsg::error, L"Untrusted TLS certificate, cannot establish a secure connection with the server. If you are brave, rerun %s with the --insecure option.", argv[0]);
				return false;
			}

			return true;
		})
		.default_timeout(timeout)
	);

	c.perform("GET", fz::uri(url)).and_then([&](http::response::status status, http::response &r) {
		std::cout << r.body.to_view();

		if (status == http::response::got_end)
			loop.stop();
		else
		if (status == http::response::got_body)
			r.body.clear();

		return 0;
	});

	loop.run();

	return 0;
}
