#include "password_with_impersonation.hpp"

namespace fz::authentication::password {

bool with_impersonation::verify(std::string_view username, std::string_view password, impersonation_token &token) const
{
	if (get_impersonation()) {
		impersonation_token t(fz::to_native(username), fz::to_native(password));

		return bool(token = std::move(t));
	}

	token = {};

	if (get())
		return password_.verify(password);

	return false;
}

}
