#ifndef FZ_DEBUG_HPP
#define FZ_DEBUG_HPP

#ifndef ENABLE_FZ_DEBUG
#   define ENABLE_FZ_DEBUG 0
#endif

#define FZ_DEBUG_LOG_DISABLED(...) do {} while (false)

#if ENABLE_FZ_DEBUG
#   include "../filezilla/logger/stdio.hpp"
#   include "../filezilla/util/demangle.hpp"
#   include "../filezilla/preprocessor/cat.hpp"

	namespace fz::debug {

		inline fz::logger::stdio logger(stderr, logmsg::debug_debug);

		template <typename... Args>
		auto log(const char *module)
		{
			return [module](auto &&... args) {
				logger.log_u(logmsg::debug_debug, L"[%s] %s", module, fz::sprintf(std::forward<decltype(args)>(args)...));
			};
		}

	}

#   define FZ_DEBUG_LOG(module, enabled) FZ_PP_CAT(FZ_INTERNAL_DEBUG_LOG_, enabled)(module)

#   define FZ_INTERNAL_DEBUG_LOG_1(module) ::fz::debug::log(module)
#   define FZ_INTERNAL_DEBUG_LOG_0(module) FZ_DEBUG_LOG_DISABLED
#else
#   define FZ_DEBUG_LOG(module, enabled) FZ_DEBUG_LOG_DISABLED
#endif

#endif // DEBUG_HPP
