#ifndef FZ_LOGGER_SCOPED_HPP
#define FZ_LOGGER_SCOPED_HPP

#include <tuple>

#include <libfilezilla/logger.hpp>
#include "../util/scope_guard.hpp"
#include "../preprocessor/cat.hpp"

namespace fz::logger
{

template <typename ConFmt, typename DesFmt, typename... Args>
auto scoped(logger_interface &logger, logmsg::type type, ConFmt && con_fmt, DesFmt && des_fmt, Args &&... args)
{
	logger.log_u(type, std::forward<ConFmt>(con_fmt), args...);

	return util::scope_guard([&logger, type, t = std::tuple<DesFmt, Args...>(std::forward<DesFmt>(des_fmt), std::forward<Args>(args)...)] {
		std::apply([&](auto &&... args) {
			logger.log_u(type, std::forward<decltype(args)>(args)...);
		}, t);
	});
}

#define FZ_LOGGER_FUNCTION(l, type) \
	auto FZ_PP_CAT(FZ_PP_CAT(fz_logger_function_auto_var_, __LINE__), _) = fz::logger::scoped(l, type, ">>> Entering %s", "<<< Leaving %s", __func__)
/***/

}

#endif // FZ_LOGGER_SCOPED_HPP
