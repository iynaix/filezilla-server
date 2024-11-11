#include <cstring>
#include <libfilezilla/impersonation.hpp>

#include "../../filezilla/util/filesystem.hpp"
#include "../../filezilla/logger/file.hpp"
#include "../../filezilla/logger/null.hpp"

#include "../../filezilla/impersonator/client.hpp"
#include "../../filezilla/impersonator/server.hpp"

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "../../filezilla/tls_exit.hpp"

#if 0
int main(int argc, char *argv[])
{
	fz::logger::stdio logger(stderr);
	logger.set_all(fz::logmsg::type(~0));

	int in_fd = -1;
	int out_fd = -1;

	if (argc == 2) {
		auto user = getenv("USER");
		auto pass = getenv("PASS");

		if (!user || !*user || !pass || !*pass) {
			logger.log_u(fz::logmsg::error, L"Pass username and password in the USER and PASS environment variables");
			return EXIT_FAILURE;
		}

		fz::impersonation_token token(fz::to_native(user), fz::to_native(pass));

		fz::impersonator::client c(logger, std::move(token), fz::to_native(argv[0]));

		logger.log_u(fz::logmsg::status, L"Trying to open file \"%s\" with impersonated user \"%s\".", argv[1], user);
		fz::impersonator::fd_owner fd;

		fz::result res;
		c.open_file(fz::to_native(argv[1]), fz::file::writing, fz::file::existing, fz::sync_receive >> std::tie(res, fd));
		logger.log_u(fz::logmsg::status, L"result is: %d, fd: %d", int(res.error_), intptr_t(fd.get()));

		fz::file f(fd.release());
		if (f) {
			logger.log_u(fz::logmsg::status, L"Writing to file.");
			f.write("ciao\n", 5);
		}

		auto dirname = fz::util::fs::native_path(fz::to_native(argv[1])).parent().str();

		logger.log_u(fz::logmsg::status, L"Trying to open directory \"%s\" with impersonated user \"%s\".", dirname, user);
		c.open_directory(dirname, fz::sync_receive >> std::tie(res, fd));
		logger.log_u(fz::logmsg::status, L"result is: %d, fd: %d", int(res.error_), intptr_t(fd.get()));

		if (fd) {
			logger.log_u(fz::logmsg::status, L"Reading directory.");

			fz::local_filesys lf;
			auto res = lf.begin_find_files(fd.release());
			if (res) {
				fz::native_string name;

				SetLastError(0);
				while (lf.get_next_file(name)) {
					logger.log_u(fz::logmsg::reply, L"  %s", name);
				}
				logger.log_u(fz::logmsg::status, L"Exiting with success: %d", GetLastError());
				FZ_EXIT(EXIT_SUCCESS);
			}
			else
				logger.log_u(fz::logmsg::error, L"Error in begin_find_files(): %d, raw: %d", res.error_, res.raw_);
		}

		logger.log_u(fz::logmsg::status, L"Exiting with failure");
		FZ_EXIT(EXIT_FAILURE);
	}
	else
	if (argc != 4 || strcmp(argv[1], "MAGIC_VALUE!") || (in_fd = fz::to_integral(argv[2], -1)) == -1 || (out_fd = fz::to_integral(argv[3], -1)) == -1) {
		FZ_EXIT(EXIT_FAILURE);
	}

	auto l = fz::logger::file::options()
		.name(fzT("C:\impersonator.log"))
		.enabled_types(fz::logmsg::type(~0UL))
		.construct();

	FZ_EXIT(fz::impersonator::server(l, in_fd, out_fd).run());
}
#else
int main(int argc, char *argv[])
{
#ifdef __linux__
	// Set PR_SET_DUMPABLE to SUID_DUMP_DISABLE, this effectively disallows ptrace by
	// a different process running under the same effective user. While not perfect,
	// it is useful in case the kernel.yama.ptrace_scope syctl knob is 0. Note that
	// it inherently racy, a tracer might still attach between the parent's call to
	// execve and this call to prctl.
	if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
		return EXIT_FAILURE;
	}
#endif
	int in_fd = -1;
	int out_fd = -1;

	if (argc != 4 || strcmp(argv[1], "MAGIC_VALUE!") || (in_fd = fz::to_integral(argv[2], -1)) == -1 || (out_fd = fz::to_integral(argv[3], -1)) == -1) {
		fz::logger::stdio(stderr).log_u(fz::logmsg::error, L"Wrong number of parameters.");
		return EXIT_FAILURE;
	}

	fz::tls_exit(fz::impersonator::server(fz::logger::null, in_fd, out_fd).run());
}
#endif
