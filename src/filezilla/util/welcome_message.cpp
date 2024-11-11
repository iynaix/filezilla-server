#include <libfilezilla/string.hpp>

#include "welcome_message.hpp"

namespace fz::util {

welcome_message::validate_result welcome_message::validate() const
{
	if (size() > validate_result::total_limit)
		return {validate_result::total_size_too_big, *this};

	for (auto &l: fz::strtokenizer(*this, "\r\n", true)) {
		if (l.size() > validate_result::line_limit)
			return {validate_result::line_too_long, l};
	}

	return {validate_result::ok, *this};
}

}
