#ifndef FZ_STRSYSERROR_HPP
#define FZ_STRSYSERROR_HPP

#include <libfilezilla/string.hpp>

namespace fz {

#ifdef FZ_WINDOWS
	using syserror_type = unsigned int;
#else
	using syserror_type = int;
#endif

/// Convert a "system error" into a native string.
/// A "system error" is the one returned by GetLastError() on Windows™, errno otherwise.
/// The function is thread safe.

native_string strsyserror(syserror_type error);

}

#endif // FZ_STRSYSERROR_HPP
