#ifndef FZ_LOGGER_TYPE_HPP
#define FZ_LOGGER_TYPE_HPP

#include <libfilezilla/logger.hpp>

namespace fz::logmsg
{
	static inline constexpr type warning = logmsg::custom1;
	static inline constexpr type default_types = type(status | command | error | reply | warning);
}

namespace fz::logger
{

template <typename String, typename Ch = std::decay_t<decltype(std::declval<String>()[0])>>
const decltype(String(fzS(Ch, "")))& type2str(fz::logmsg::type t, bool short_format = true)
{
	if (short_format) {
		switch(t) {
			case logmsg::status:        { static const auto v = String(fzS(Ch, "==")); return v; }
			case logmsg::error:         { static const auto v = String(fzS(Ch, "!!")); return v; }
			case logmsg::command:       { static const auto v = String(fzS(Ch, ">>")); return v; }
			case logmsg::reply:         { static const auto v = String(fzS(Ch, "<<")); return v; }
			case logmsg::warning:       { static const auto v = String(fzS(Ch, "WW")); return v; }
			case logmsg::debug_warning: { static const auto v = String(fzS(Ch, "DW")); return v; }
			case logmsg::debug_info:    { static const auto v = String(fzS(Ch, "DI")); return v; }
			case logmsg::debug_verbose: { static const auto v = String(fzS(Ch, "DV")); return v; }
			case logmsg::debug_debug:   { static const auto v = String(fzS(Ch, "DD")); return v; }

			case logmsg::custom32: {
				static const auto v = String(fzS(Ch, "PP")); return v;
			}
		}

		if (t) {
			static const auto v = String(fzS(Ch, "PP")); return v;
		}
	}
	else {
		switch(t) {
			case logmsg::status:        { static const auto v = String(fzS(Ch, "Status:")); return v; }
			case logmsg::error:         { static const auto v = String(fzS(Ch, "Error:")); return v; }
			case logmsg::command:       { static const auto v = String(fzS(Ch, "Command:")); return v; }
			case logmsg::reply:         { static const auto v = String(fzS(Ch, "Reply:")); return v; }
			case logmsg::warning:       { static const auto v = String(fzS(Ch, "Warning:")); return v; }
			case logmsg::debug_warning: { static const auto v = String(fzS(Ch, "Debug Warning:")); return v; }
			case logmsg::debug_info:    { static const auto v = String(fzS(Ch, "Debug Info:")); return v; }
			case logmsg::debug_verbose: { static const auto v = String(fzS(Ch, "Debug Verbose:")); return v; }
			case logmsg::debug_debug:   { static const auto v = String(fzS(Ch, "Debug Debug:")); return v; }

			case logmsg::custom32: {
				static const auto v = String(fzS(Ch, "Custom: ")); return v;
			}
		}

		if (t) {
			static const auto v = String(fzS(Ch, "Custom: ")); return v;
		}
	}

	static const auto v = String(fzS(Ch, ""));
	return v;
}

}
#endif // FZ_LOGGER_TYPE_HPP
