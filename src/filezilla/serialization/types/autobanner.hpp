#ifndef FZ_SERIALIZATION_TYPES_AUTOBANNER_HPP
#define FZ_SERIALIZATION_TYPES_AUTOBANNER_HPP

#include "optional.hpp"
#include "../../authentication/autobanner.hpp"

namespace fz::serialization {

template <typename Archive>
void serialize(Archive &ar, struct authentication::autobanner::options &o)
{
	using namespace serialization;

	ar(
		value_info(optional_nvp(o.ban_duration(),
				   "ban_duration"),
				   "The duration, in milliseconds, of the IP ban."),

		value_info(optional_nvp(o.login_failures_time_window(),
				   "login_failure_time_window"),
				   "The duration, in milliseconds, during which the number of failed login attempts is monitored."),

		value_info(optional_nvp(o.max_login_failures(),
				   "login_failure_time_window"),
				   "The number of login attempts that are allowed to fail, within the time window specified by the parameter [login_failures_time_window]. "
				   "The value 0 disables this mechanism.")
	);
}

}


#endif // FZ_SERIALIZATION_TYPES_AUTOBANNER_HPP
