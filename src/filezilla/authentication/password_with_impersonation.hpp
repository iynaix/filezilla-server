#ifndef FZ_FILEZILLA_AUTHENTICATION_PASSWORD_WITH_IMPERSONATION_HPP
#define FZ_FILEZILLA_AUTHENTICATION_PASSWORD_WITH_IMPERSONATION_HPP

#include <string>

#include <libfilezilla/impersonation.hpp>

#include "password.hpp"

namespace fz::authentication::password
{

struct with_impersonation
{
	struct impersonation {
		bool login_only{};

		explicit impersonation(bool login_only = false)
			: login_only(login_only)
		{}

		template <typename Archive>
		void serialize(Archive &ar)
		{
			ar.optional_attribute(login_only, "login_only");
		}
	};

	with_impersonation(impersonation imp)
		: impersonation_(std::move(imp))
		, password_()
	{}

	template <typename T, std::enable_if_t<std::is_constructible_v<any_password, T>>* = nullptr>
	with_impersonation(T && pwd)
		: impersonation_()
		, password_(std::forward<T>(pwd))
	{}

	void impersonate(bool v = false)
	{
		*this = with_impersonation(impersonation(v));
	}

	with_impersonation() = default;
	with_impersonation(with_impersonation &&) = default;
	with_impersonation(const with_impersonation &) = default;
	with_impersonation& operator=(with_impersonation &&) = default;
	with_impersonation& operator=(const with_impersonation &) = default;

	const any_password *get() const
	{
		return password_ ? &password_ : nullptr;
	}

	any_password *get()
	{
		return password_ ? &password_ : nullptr;
	}

	const impersonation *get_impersonation() const
	{
		return !password_ && impersonation_ ? &*impersonation_ : nullptr;
	}

	bool verify(std::string_view username, std::string_view password, impersonation_token &token) const;

	explicit operator bool() const
	{
		return password_ || impersonation_;
	}

private:
	friend fz::serialization::access;

	template <typename Archive>
	void serialize(Archive &ar)
	{
		using namespace fz::serialization;

		ar(nvp(impersonation_, "impersonation"));
		if (impersonation_) {
			password_ = {};
		}
		else {
			ar(optional_nvp(password_, "password"));
		}
	}

	std::optional<impersonation> impersonation_;
	any_password password_ = default_password();
};

}

#endif // FZ_FILEZILLA_AUTHENTICATION_PASSWORD_WITH_IMPERSONATION_HPP
