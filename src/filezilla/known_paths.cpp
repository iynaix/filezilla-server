#include <cstdlib>

#include <libfilezilla/local_filesys.hpp>

#include "known_paths.hpp"

#include "util/io.hpp"
#include "util/tools.hpp"

#if defined(FZ_WINDOWS)
#	include <libfilezilla/glue/windows.hpp>
#	include <libfilezilla/glue/dll.hpp>
#	include <libfilezilla/impersonation.hpp>
#	include <shlobj.h>
#elif defined(FZ_UNIX)
#	include <wordexp.h>
#elif defined(FZ_MAC)
#	include <Foundation/NSFileManager.h>
#	include <Foundation/NSURL.h>
#endif

namespace fz::known_paths {

namespace {

	#if defined (FZ_WINDOWS)
		namespace folderid {

			static constexpr GUID profile       = { 0x5E6C858F, 0x0E22, 0x4760, {0x9A, 0xFE, 0xEA, 0x33, 0x17, 0xB6, 0x71, 0x73} };
			static constexpr GUID local_appdata = { 0xF1B32785, 0x6FBA, 0x4FCF, {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91} };
			static constexpr GUID program_data  = { 0x62AB5D82, 0xFDC1, 0x4DC3, {0xA9, 0xDD, 0x07, 0x0D, 0x1D, 0x49, 0x5D, 0x97} };
			static constexpr GUID windows       = { 0xF38BF404, 0x1D43, 0x42F2, {0x93, 0x05, 0x67, 0xDE, 0x0B, 0x28, 0xFC, 0x23} };
			static constexpr GUID downloads     = { 0x374DE290, 0x123F, 0x4565, {0x91, 0x64, 0x39, 0xC4, 0x92, 0x5E, 0x46, 0x7B} };

		}
	#endif

	path_list empty_list() { return {}; }
	path &&same_path(path &&p) { return std::move(p); }
	path_list absolute_filter(native_string_view dirs){
		path_list ret;

		#if defined(FZ_WINDOWS)
			auto sep = fzT(";");
		#else
			auto sep = fzT(":");
		#endif

		for (auto e: strtokenizer(dirs, sep, true)) {
			if (path p{e}; p.is_absolute())
				ret.push_back(std::move(p));
		}

		return ret;
	}

#if defined (FZ_WINDOWS)
	std::wstring get(const GUID &var)
	{
		std::wstring value;

		static auto get = reinterpret_cast<decltype(&SHGetKnownFolderPath)>(shdlls::get().shell32_["SHGetKnownFolderPath"]);
		static auto free = reinterpret_cast<decltype(&CoTaskMemFree)>(shdlls::get().ole32_["CoTaskMemFree"]);

		if (get && free) {
			wchar_t *wret{};

			if (SUCCEEDED(get(var, 0, nullptr, &wret)))
				value = native_string(wret);

			free(wret);
		}

		return value;
	}
#elif defined (FZ_UNIX)
	std::string get(const char *var)
	{
		std::string value;

		auto v = getenv(var);
		if (v && v[0])
			value = v;

		if (value.empty() && fz::starts_with(std::string_view(var), std::string_view("XDG_")) && fz::ends_with(std::string_view(var), std::string_view("_DIR"))) {
			if (auto buf = fz::util::io::read(config::home() / fzT("user-dirs.dirs"))) {
				for (auto l: strtokenizer(buf.to_view(), "\n", true)) {
					if (auto kv = strtok_view(l, "=", false); kv.size() == 2 && kv[0] == var) {
						::wordexp_t p;

						if (int res = ::wordexp(std::string(kv[1]).c_str(), &p, ::WRDE_NOCMD); res == 0 && p.we_wordc == 1 && p.we_wordv)
							value = p.we_wordv[0];

						::wordfree(&p);

						if (!value.empty())
							break;
					}
				}

			}
		}

		return value;
	}
#elif defined (FZ_MAC)
	std::string get(const char *var)
	{
		std::string value;

		auto v = getenv(var);
		if (v && v[0])
			value = v;


		return value;
	}

	std::string get(NSSearchPathDirectory e)
	{
		NSURL* url = [[NSFileManager defaultManager] URLForDirectory:e inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:nil];
		if (url)
			return url.path.UTF8String;

		return {};
	}
#endif

	template <typename Var, typename IfFunc, typename ElseFunc>
	auto get(Var &&var, IfFunc if_func, ElseFunc else_func) -> std::decay_t<decltype (if_func(fz::native_string()))>
	{
		native_string value = get(var);

		if (!value.empty())
			return if_func(std::move(value));

		return else_func();
	}

	#if defined(FZ_UNIX)
	#	define var(Unix, Mac, Windows) Unix
	#elif defined(FZ_MAC)
	#	define var(Unix, Mac, Windows) Mac
	#elif defined(FZ_WINDOWS)
	#	define var(Unix, Mac, Windows) Windows
	#else
	#   error "Your platform is not supported yet."
	#endif
}

const path &data::home() {
	static const path p = get(var("XDG_DATA_HOME", "XDG_DATA_HOME", folderid::local_appdata), same_path, []{ return user::home() / fzT(".local") / fzT("share"); });
	return p;
}

const path_list &data::dirs() {
	static const path_list pl = get(var("XDG_DATA_DIRS", "XDG_DATA_DIRS", folderid::local_appdata), absolute_filter, []() -> path_list {
	#if defined(FZ_UNIX)
		return absolute_filter("/usr/local/share/:/usr/share/");
	#elif defined(FZ_WINDOWS)
		return absolute_filter(util::get_own_executable_directory());
	#else
		return empty_list();
	#endif
	});

	return pl;
}

const path &config::home() {
	static const path p = get(var("XDG_CONFIG_HOME", "XDG_CONFIG_HOME", folderid::local_appdata), same_path, []{ return user::home() / fzT(".config"); });
	return p;
}

const path_list &config::dirs() {
	static const path_list pl = get(var("XDG_CONFIG_DIRS", "XDG_CONFIG_DIRS", folderid::local_appdata), absolute_filter, empty_list);
	return pl;
}

const path &config::effective_home()
{
#if defined(FZ_WINDOWS)
	bool is_system = fz::stricmp(fz::current_username(), L"SYSTEM") == 0;

	if (is_system)
		return windows::program_data();

#endif
	return home();
}

namespace user {
	const path &home()  {
		static const path p = get(var("HOME", "HOME",folderid::profile), same_path, []{ return native_string(1, local_filesys::path_separator); });
		return p;
	}

	const path &downloads()
	{
		static const path l = get(var("XDG_DOWNLOAD_DIR", NSDownloadsDirectory, folderid::downloads), same_path, home);
		return l;
	}
}

detail::base::base(const path &path_for_writing, const path_list &paths_for_reading)
	: path_for_writing_(path_for_writing)
	, paths_for_reading_(paths_for_reading)
{
	paths_for_reading_.erase(std::remove_if(paths_for_reading_.begin(), paths_for_reading_.end(), [](const auto &p) {
		return p.type() != fz::local_filesys::dir;
	}), paths_for_reading_.end());
}

#if defined(FZ_WINDOWS)
namespace windows {
	const path &program_data()
	{
		static const path p = get(folderid::program_data, same_path, config::home);
		return p;
	}

	const path &windir()
	{
		static const path p = get(folderid::windows, same_path, [] { return L"C:\\Windows"; });
		return p;
	}
}
#endif

}

