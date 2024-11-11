#include "credentials.hpp"
#include "../logger/type.hpp"

namespace fz::authentication {

bool credentials::verify(std::string_view username, const any_method &method, impersonation_token &impersonation, logger_interface &logger) const
{
	impersonation_token it;

	bool success{};

	if (auto m = method.is<method::password>()) {
		success = password.verify(username, m->data, it);
	}
	else
	if (auto m = method.is<method::token>()) {
		success = m->manager.get().verify(username, m->data, it);
	}

	if (success && it) {
		if (impersonation) {
			logger.log_raw(logmsg::error, L"Conflict: we already had an impersonation token, and there can be only one.");
			return false;
		}

		impersonation = std::move(it);
	}

	return success;
}

bool credentials::is_valid_for(const available_methods &m, logger_interface *l) const
{
	auto warn = [&](auto &&... args) {
		if (l) {
			l->log_u(logmsg::warning, std::forward<decltype(args)>(args)...);
		}

		return false;
	};

	if (m.is_auth_necessary()) {
		if (m.can_verify(method::password()) && !password) {
			return warn(L"Auth method 'password' is required but no password is available");
		}
	}

	return true;
}

available_methods credentials::get_most_secure_methods() const
{
	methods_set ms;

	if (password) {
		ms.add<method::password>();
	}

	return { ms };
}

}
