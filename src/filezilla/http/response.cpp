#include <libfilezilla/format.hpp>

#include "response.hpp"

namespace fz::http
{

std::string response::code_string() const
{
	const char *format = [&] {
		switch (code_type())
		{
			case informational:
				return "Informational (%d)";

			case successful:
				return "Successful (%d)";

			case redirect:
				return "Redirect (%d)";

			case client_error:
				return "Client error (%d)";

			case server_error:
				return "Server error (%d)";

			case internal_error:
				return "Internal error";
		}

		return "Unknown (%d)";
	}();

	return fz::sprintf(format, code);
}

}
