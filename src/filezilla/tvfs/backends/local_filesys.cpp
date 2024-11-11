#if !defined(FZ_WINDOWS)
#	include "fcntl.h"
#endif

#include <libfilezilla/recursive_remove.hpp>

#include "../../strsyserror.hpp"
#include "local_filesys.hpp"
#include "../../strresult.hpp"

namespace fz::tvfs::backends {

local_filesys::local_filesys(logger_interface &logger)
	: logger_(logger, "local_filesys")
{
}

void local_filesys::open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r)
{
	fz::file f;

	auto res = !native_path
		? result { result::invalid }
		: f.open(native_path, mode, flags);

	logger_.log_u(logmsg::debug_debug, L"open_file(%s): fd = %d, res = %d (raw = %d: %s)", native_path, std::intptr_t(f.fd()), res.error_, res.raw_, strsyserror(res.raw_));

	return r(res, std::move(f));
}

void local_filesys::open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r)
{
	fd_owner fd;
	result res;

	if (!native_path)
		res = {result::invalid};
	else {
		#if defined(FZ_WINDOWS)
			fd = fd_owner(CreateFileW(native_path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
			auto last_error = GetLastError();
		#else
			fd = fd_owner(::open(native_path.c_str(), O_DIRECTORY|O_RDONLY|O_CLOEXEC));
			auto last_error = errno;
		#endif

		if (!fd) {
			switch (last_error) {
				FZ_RESULT_RAW_CASE_WIN(ERROR_ACCESS_DENIED)
				FZ_RESULT_RAW_CASE_NIX(EACCES)
				FZ_RESULT_RAW_CASE_NIX(EPERM)
					res = { result::noperm, last_error };
					break;

				FZ_RESULT_RAW_CASE_WIN(ERROR_PATH_NOT_FOUND)
				FZ_RESULT_RAW_CASE_WIN(ERROR_FILE_NOT_FOUND)
				FZ_RESULT_RAW_CASE_NIX(ENOENT)
				FZ_RESULT_RAW_CASE_NIX(ENOTDIR)
					res = { result::nodir, last_error };
					break;

				default:
					res = { result::other, last_error };
			}
		}

		if (fd)
			res = { result::ok };
	}

	logger_.log_u(logmsg::debug_debug, L"open_directory(%s): fd = %d, res = %d (raw = %d: %s)", native_path, std::intptr_t(fd.get()), res.error_, res.raw_, strsyserror(res.raw_));

	return r(res, std::move(fd));
}

void local_filesys::rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r)
{
	result res;

	if (!path_from || !path_to)
		res = {result::invalid};
	else
		res = fz::rename_file(path_from, path_to);

	logger_.log_u(logmsg::debug_debug, L"rename(%s, %s): result: %d (raw = %d: %s)", path_from, path_to, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res);
}

void local_filesys::remove_file(const absolute_native_path &path, receiver_handle<remove_response> r)
{
	result res;

	if (!path)
		res = { result::invalid };
	else
		res = fz::remove_file(path, true);

	logger_.log_u(logmsg::debug_debug, L"remove_file(%s): result: %d (raw = %d: %s)", path, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res);
}

void local_filesys::remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r)
{
	result res;

	if (!path) {
		res = { result::invalid };
	}
	else
	if (recursive) {
		if (fz::recursive_remove().remove(path)) {
			res = { result::ok };
		}
		else {
			res = { result::other };
		}
	}
	else {
		res = fz::remove_dir(path, true);
	}

	logger_.log_u(logmsg::debug_debug, L"remove_directory(%s): result: %d (raw = %d: %s)", path, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res);
}

void local_filesys::info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r)
{
	result res;

	bool is_link;
	fz::local_filesys::type type = fz::local_filesys::unknown;
	int64_t size;
	datetime modification_time;
	int mode;

	if (!path)
		res = { result::invalid };
	else {
		type = fz::local_filesys::get_file_info(path, is_link, &size, &modification_time, &mode, follow_links);

		res = result{ type == fz::local_filesys::unknown ? result::other : result::ok };
	}

	logger_.log_u(logmsg::debug_debug, L"info(%s): result: %d (raw = %d: %s)", path, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res, is_link, type, size, modification_time, mode);
}

void local_filesys::mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r)
{
	result res;
	native_string last_created;

	if (!path)
		res = { result::invalid };
	else
		res = fz::mkdir(path, recurse, permissions, &last_created);

	if (res && last_created.empty()) {
		res = { result::other, FZ_RESULT_RAW(ERROR_ALREADY_EXISTS, EEXIST) };
	}

	logger_.log_u(logmsg::debug_debug, L"mkdir(%s): result: %d (raw = %d: %s)", path, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res);
}

void local_filesys::set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r)
{
	result res;

	if (!path)
		res = { result::invalid };
	else
	if (fz::local_filesys::set_modification_time(path, mtime))
		res = { result::ok };
	else
		res = { result::other };

	logger_.log_u(logmsg::debug_debug, L"set_mtime(%s): result: %d (raw = %d: %s)", path, res.error_, res.raw_, strsyserror(res.raw_));

	return r(res);
}

}
