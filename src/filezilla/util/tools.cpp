#include <libfilezilla/libfilezilla.hpp>

#if defined(FZ_MAC)
#	include <mach-o/dyld.h>
#	include <libfilezilla/string.hpp>
#elif defined(FZ_WINDOWS)
#	include <shlobj.h>
#	include <objbase.h>
#elif defined(FZ_UNIX)
#	include <libfilezilla/string.hpp>
#else
#	error "Which platform are you compiling for?"
#endif

#include <unistd.h>

#include "tools.hpp"

namespace fz::util {

fs::native_path get_own_executable_directory()
{
	fz::native_string path;

#if defined(FZ_MAC)
	uint32_t size = 0;

	_NSGetExecutablePath(nullptr, &size);
	if (size == 0)
		return {};

	path.resize(std::size_t(size)-1);

	if (_NSGetExecutablePath(path.data(), &size) != 0)
		return {};
#else
	path.resize(1024);

	while (true) {
	#ifdef FZ_WINDOWS
		auto res = ::GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size() - 1));
	#elif defined(FZ_UNIX)
		auto res = ::readlink("/proc/self/exe", &path[0], path.size());
	#else
	#	error "Which platform are you compiling for?"
	#endif

		if (res <= 0)
			return {};

		if (size_t(res) < path.size() - 1) {
			path.resize(size_t(res));
			break;
		}

		path.resize(path.size() * 2);
	}
#endif

	return fz::util::fs::native_path(path).parent();
}


fs::native_path find_tool(fz::native_string name, fs::native_path build_rel_path, const char *env)
{
	fz::util::fs::native_path tool;

	static auto file_exists = [](const fz::util::fs::native_path &path) {
		return path.type() == fz::local_filesys::file;
	};

	auto tool_exists = [&tool](const fz::util::fs::native_path &path) {
		if (file_exists(path)) {
			tool = path;
			return true;
		}

		return false;
	};

	// First check the given environment variable
	if ((env = ::getenv(env))) {
		if (tool_exists(fz::to_native(env)))
			return tool;
	}

#if defined(FZ_WINDOWS)
	name += fzT(".exe");
#endif

	// Then check in own executable directory and, possibly, build directories.
	auto dir = get_own_executable_directory();

	if (dir.is_absolute()) {
		if (tool_exists(dir / name)) {
			return tool;
		}

		// Check if running from build dir
		if (dir.base().str() == fzT(".libs")) {
			dir.make_parent();
			build_rel_path /= fzT(".libs");
		}

		if (file_exists(dir/ fzT("Makefile"))) {
			if (tool_exists(dir / build_rel_path / name)) {
				return tool;
			}
		}
	}

	// Last but not least, PATH
	if ((env = ::getenv("PATH"))) {
	#if defined(FZ_WINDOWS)
		static const std::string_view sep = ";";
	#else
		static const std::string_view sep = ":";
	#endif
		for (auto path: fz::strtokenizer(env, sep, true)) {
			if (tool_exists(fz::util::fs::native_path(fz::to_native(path)) / name))
				return tool;
		}
	}

	return {};
}

fs::absolute_native_path get_current_directory_name()
{
	fz::native_string buf;

#ifndef FZ_WINDOWS
	if (buf.capacity() > 0) {
		buf.resize(buf.capacity()-1);
	}
	else {
		buf.resize(15);
	}

	while (!::getcwd(buf.data(), buf.size()+1)) {
		if (errno != ERANGE) {
			return {};
		}

		buf.resize((buf.size()+1)*2 - 1);
	}

	buf.resize(fz::native_string::traits_type::length(buf.data()));
#else
	auto len = ::GetCurrentDirectoryW(0, nullptr);
	if (len == 0) {
		return {};
	}

	buf.resize(len-1);
	len = ::GetCurrentDirectoryW(buf.size()+1, buf.data());
	if (len != buf.size()) {
		return {};
	}
#endif

	return buf;
}

fs::absolute_native_path get_abs_top_src_directory(fs::relative_native_path relative_top_srcdir)
{
	auto dir = get_own_executable_directory();
	if (dir.is_absolute()) {
		// Check if running from build dir
		if (dir.base().str() == fzT(".libs")) {
			dir.make_parent();
		}

		if ((dir/ fzT("Makefile")).type() == local_filesys::file) {
			return dir / relative_top_srcdir;
		}
	}

	return {};
}

}
