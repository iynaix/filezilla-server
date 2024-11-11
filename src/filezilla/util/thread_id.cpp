#include <atomic>

#include "../util/thread_id.hpp"
#include "../logger/stdio.hpp"

#if HAS_BACKTRACE
#   include <execinfo.h>
#endif

namespace fz::util {

std::size_t thread_id()
{
	static std::atomic<std::size_t> next_thread_id = 0;
	thread_local std::size_t own_thread_id = ++next_thread_id;

	return own_thread_id;
}

void thread_check::operator()(const char *func)
{
	if (scoped_lock lock(mutex_); expected_thread_id_ == 0) {
		expected_thread_id_ = thread_id();
		expected_func_ = func;

		#if HAS_BACKTRACE
			num_of_callstack_entries_ = ::backtrace(callstack_, sizeof(callstack_)/sizeof(callstack_[0]));
		#endif
	}
	if (expected_thread_id_ != thread_id()) {
		fz::logger::stdio logger(stderr);
		logger.log_u(logmsg::error, L"Expected thread [%d] doesn't match with current thread [%d].", expected_thread_id_, thread_id());
		logger.log_u(logmsg::error, L"Expected thread [%d] was set in function [%s].", expected_thread_id_, expected_func_);
		#if HAS_BACKTRACE
			dump_callstack(logger, expected_func_, callstack_, num_of_callstack_entries_);
		#endif

		logger.log_u(logmsg::error, L"Current function is [%s]", func);
		#if HAS_BACKTRACE
			decltype(callstack_) callstack;
			auto num_entries = ::backtrace(callstack, sizeof(callstack)/sizeof(callstack[0]));
			dump_callstack(logger, func, callstack, num_entries);
		#endif

		logger.log_u(logmsg::error, L"Aborting.");

		abort();
	}
}

#if HAS_BACKTRACE
void thread_check::dump_callstack(logger_interface &logger, const char *func, void **callstack, int num_entries)
{
	if (num_entries > 1) {
		logger.log_u(logmsg::error, L"Backtrace for funcion [%s] follows:", func);
		auto syms = ::backtrace_symbols(callstack, num_entries);
		for (int i = 1; i < num_entries; ++i)
			logger.log_u(logmsg::error, L">    %s", syms[i]);
		::free(syms);
	}
}
#endif


}
