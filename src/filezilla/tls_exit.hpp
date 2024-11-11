#ifndef FZ_TLS_EXIT_HPP
#define FZ_TLS_EXIT_HPP

/*
 * Mingw32 has issues with de-initializing TLS variables during program exit: in short, the program crashes.
 * fz::tls_exit is just a shortcut to avoid going through the TLS deinitializations on exit.
 * Use it where you would use std::exit(). On Windows, it will do the right thing.
 *
 * The windows implementation could use _exit(), but that one still crashes, so win32-specific calls are used instead.
 */

#include <cstdlib>
#include <cstdio>

#if defined(__MINGW32__)
#	include <processthreadsapi.h>
#endif

namespace fz {

[[noreturn]] inline void tls_exit(int x) {
#if defined(__MINGW32__)
	std::fflush(stdout);
	std::fflush(stderr);
	::TerminateProcess(::GetCurrentProcess(), x);
	// This will never be reached, but informs the compiler that the function doesn't, in fact, return.
	::std::abort();
#else
	::std::exit(x);
#endif
}

}

#endif // FZ_TLS_EXIT_HPP
