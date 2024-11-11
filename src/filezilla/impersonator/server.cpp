#include "server.hpp"
#include "parent_proxy.hpp"
#include "../util/locking_wrapper.hpp"

#if !defined(FZ_WINDOWS)
#	include <fcntl.h>
#endif

fz::impersonator::server::server(fz::logger_interface &logger, int in_fd, int out_fd)
	: logger_(logger, "impersonator server")
	, channel_(event_loop_, logger_, std::make_unique<fz::impersonator::parent_proxy>(logger_, in_fd, out_fd))
	, backend_(logger_)
{
}

int fz::impersonator::server::run()
{
	fz::impersonator::any_message any;

	logger_.log_u(fz::logmsg::debug_info, L"run(): receiving any_message");

	while (!error_ && !(error_ = channel_.recv(any))) {
		logger_.log_u(fz::logmsg::debug_info, L"run(): dispatching any_message");

		bool dispatched = fz::rmp::dispatch<
			fz::impersonator::messages::open_file,
			fz::impersonator::messages::open_directory,
			fz::impersonator::messages::rename,
			fz::impersonator::messages::remove_file,
			fz::impersonator::messages::remove_directory,
			fz::impersonator::messages::info,
			fz::impersonator::messages::mkdir,
			fz::impersonator::messages::set_mtime
		>(std::move(any), this,
			&server::open_file,
			&server::open_directory,
			&server::rename,
			&server::remove_file,
			&server::remove_directory,
			&server::info,
			&server::mkdir,
			&server::set_mtime
		);

		if (!dispatched) {
			auto msg = fz::sprintf(L"run(): message not implemented (index: %d)", any.index());
			logger_.log_u(fz::logmsg::debug_warning, msg);
			error_ = channel_.send(rmp::exception::generic(fz::to_utf8(msg)));
		}
	}

	if (error_ != ENODATA) {
		logger_.log_u(fz::logmsg::error, L"run(): %s (%d)", std::strerror(error_), error_);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void fz::impersonator::server::open_file(const fz::native_string &native_path, fz::file::mode mode, fz::file::creation_flags flags)
{
	backend_.open_file(native_path, mode, flags, sync_receive >> [&] (auto &res, auto &fd) {
		error_ = channel_.send(fz::impersonator::messages::open_response(res, std::move(fd)));
	});
}

void fz::impersonator::server::open_directory(const fz::native_string &native_path)
{
	backend_.open_directory(native_path, sync_receive >> [&](auto &res, auto &fd) {
		error_ = channel_.send(fz::impersonator::messages::open_response(res, std::move(fd)));
	});
}

void fz::impersonator::server::rename(const fz::native_string &from, const fz::native_string &to)
{
	backend_.rename(from, to, sync_receive >> [&](auto &res) {
		error_ = channel_.send(fz::impersonator::messages::rename_response(res));
	});
}

void fz::impersonator::server::remove_file(const fz::native_string &path)
{
	backend_.remove_file(path, sync_receive >> [&](auto &res) {
		error_ = channel_.send(fz::impersonator::messages::remove_response(res));
	});
}

void fz::impersonator::server::remove_directory(const fz::native_string &path, bool recursive)
{
	backend_.remove_directory(path, recursive, sync_receive >> [&](auto &res) {
		error_ = channel_.send(fz::impersonator::messages::remove_response(res));
	});
}

void fz::impersonator::server::info(const fz::native_string &path, bool follow_links)
{
	backend_.info(path, follow_links, sync_receive >> [&](auto &res, auto &is_link, auto &type, auto &size, auto &dt, auto &mode) {
		error_ = channel_.send(fz::impersonator::messages::info_response(res, is_link, type, size, dt, mode));
	});
}

void fz::impersonator::server::mkdir(const fz::native_string &path, bool recurse, fz::mkdir_permissions mkdir_permissions)
{
	backend_.mkdir(path, recurse, mkdir_permissions, sync_receive >> [&](auto &res) {
		error_ = channel_.send(fz::impersonator::messages::mkdir_response(res));
	});
}

void fz::impersonator::server::set_mtime(const fz::native_string &path, const fz::datetime &mtime)
{
	backend_.set_mtime(path, mtime, sync_receive >> [&](auto &res) {
		error_ = channel_.send(fz::impersonator::messages::set_mtime_response(res));
	});
}










































