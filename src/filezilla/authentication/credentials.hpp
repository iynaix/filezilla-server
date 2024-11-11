#ifndef FZ_AUTHENTICATION_CREDENTIALS_HPP
#define FZ_AUTHENTICATION_CREDENTIALS_HPP

#include <libfilezilla/impersonation.hpp>
#include <libfilezilla/logger.hpp>

#include "password_with_impersonation.hpp"
#include "method.hpp"

#include "../serialization/helpers.hpp"

namespace fz::authentication {

class credentials {
public:
	bool verify(std::string_view username, const any_method &method, impersonation_token &token, logger_interface &logger = get_null_logger()) const;
	bool is_valid_for(const available_methods &methods, logger_interface *logger = nullptr) const;
	available_methods get_most_secure_methods() const;

public:
	template <typename Archive>
	void serialize(Archive &ar)
	{
		using namespace fz::serialization;

		ar(optional_nvp(password, ""));
	}

	password::with_impersonation password;
};

}

#endif // FZ_AUTHENTICATION_CREDENTIALS_HPP
