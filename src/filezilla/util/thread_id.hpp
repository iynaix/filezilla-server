#ifndef FZ_UTIL_THREAD_ID_HPP
#define FZ_UTIL_THREAD_ID_HPP

#include <libfilezilla/mutex.hpp>

#ifndef HAS_BACKTRACE
#   ifdef __linux__
#      define HAS_BACKTRACE 1
#   endif
#else
#   define HAS_BACKTRACE 0
#endif

namespace fz {

class logger_interface;

}

namespace fz::util {

std::size_t thread_id();

class thread_check
{
public:
	void operator()(const char *func);

private:
	size_t expected_thread_id_{};
	const char *expected_func_{};
	fz::mutex mutex_;

	#if HAS_BACKTRACE
		void *callstack_[100]{};
		int num_of_callstack_entries_{};
		void dump_callstack(logger_interface &, const char *func, void **callstack, int num_entries);
	#endif
};

#if defined(ENABLED_FZ_UTIL_THREAD_CHECK) && ENABLED_FZ_UTIL_THREAD_CHECK
#   define FZ_UTIL_THREAD_CHECK_INIT                            \
	   mutable fz::util::thread_check fz_util_thread_check__{}; \
	/***/

#   define FZ_UTIL_THREAD_CHECK                     \
	   fz_util_thread_check__(__PRETTY_FUNCTION__); \
	/***/
#else
#   define FZ_UTIL_THREAD_CHECK_INIT
#   define FZ_UTIL_THREAD_CHECK
#endif

}
#endif // FZ_UTIL_THREAD_ID_HPP
