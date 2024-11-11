#include <libfilezilla/format.hpp>
#include "strsyserror.hpp"

#ifdef FZ_WINDOWS
#	include <windows.h>
#	include <libfilezilla/string.hpp>
#else
#	include <string.h>
#endif


namespace fz {


#ifdef FZ_WINDOWS

native_string strsyserror(syserror_type error) {
	if (error == 0) {
		return fzT("No error");
	}

	wchar_t* out{};

	if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t*>(&out), 0, nullptr) != 0 && out) {
		native_string ret = out;

		// Replace all line breaks with a space
		std::replace_if(ret.begin(), ret.end(), [](wchar_t c) {
			return c == L'\r' || c == L'\n';
		}, L' ');

		// Trim excess empty chars from left and right
		fz::trim(ret);

		// Remove duplicated spaces
		ret.erase(std::unique(ret.begin(), ret.end(), [](wchar_t lhs, wchar_t rhs) {
			return lhs == L' ' && rhs == L' ';
		}), ret.end());

		LocalFree(out);
		return ret;
	}

	return sprintf(fzT("Unknown error %d"), error);
}

#else

namespace {

template <typename R>
native_string strsyserror_impl(syserror_type error) {
	static bool constexpr has_posix_strerror_r = std::is_same_v<R, int>;
	static bool constexpr has_gnu_strerror_r = std::is_same_v<R, char *>;

	if constexpr (has_posix_strerror_r) {
		native_string out(255+1, '\0');

		for (R res = ::strerror_r(error, out.data(), out.size()-1); res;) {
			if (res == -1) {
				// glibc < 2.13
				res = errno;
			}


			if (res == ERANGE) {
				out.resize(out.size()*2);
				continue;
			}

			return sprintf(fzT("Unknown error %d"), error);
		}

		out.resize(native_string::traits_type::length(out.data()));
		out.shrink_to_fit();

		return out;
	}
	else
	if constexpr (has_gnu_strerror_r) {
		char buf[512+1];
		R res = ::strerror_r(error, buf, sizeof(buf));
		return res;
	}
	else {
		static_assert(!has_posix_strerror_r && !has_gnu_strerror_r, "Unknown strerror_r variant.");
		return {};
	}
}

}

native_string strsyserror(syserror_type error) {
	if (error == 0) {
		return fzT("No error");
	}

	return strsyserror_impl<decltype(::strerror_r(std::declval<int>(), std::declval<char *>(), std::declval<std::size_t>()))>(error);
}


#endif

}
