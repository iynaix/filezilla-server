#include <libfilezilla/format.hpp>

#include "strresult.hpp"

namespace fz {

std::string_view strresult(result r)
{
	using namespace std::string_view_literals;

	switch (r.error_) {
		case result::ok:             return "No error"sv;
		case result::invalid:        return "Invalid file name or path"sv;
		case result::noperm:         return "Permission denied"sv;
		case result::nofile:         return "Couldn't open the file"sv;
		case result::nodir:          return "Couldn't open the directory"sv;
		case result::nospace:        return "No space left"sv;
		case result::resource_limit: return "Too many open files or directories"sv;
		case result::other: {
			switch (r.raw_) {
				case FZ_RESULT_RAW_NOT_IMPLEMENTED:
					return "Operation not implemented"sv;

				case FZ_RESULT_RAW_ALREADY_EXISTS:
					return "File or directory already exists"sv;

				case FZ_RESULT_RAW_TIMEOUT:
					return "Operation has timed out"sv;
			}

			return "Unknown error"sv;
		}
	}

	return {};
}

std::string_view strresult(rwresult r)
{
	using namespace std::string_view_literals;

	switch (r.error_) {
		case rwresult::none:       return "No error"sv;
		case rwresult::invalid:    return "Invalid argument"sv;
		case rwresult::nospace:    return "No space left"sv;
		case rwresult::wouldblock: return "The operation would have blocked"sv;
		case rwresult::other: {
			switch (r.raw_) {
				case FZ_RESULT_RAW_NOT_IMPLEMENTED:
					return "Operation not implemented"sv;

				case FZ_RESULT_RAW_TIMEOUT:
					return "Operation has timed out"sv;
			}

			return "Unknown error"sv;
		}
	}

	return {};
}

}
