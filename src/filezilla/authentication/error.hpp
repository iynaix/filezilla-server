#ifndef FZ_AUTHENTICATION_ERROR_HPP
#define FZ_AUTHENTICATION_ERROR_HPP

#include <libfilezilla/string.hpp>

#include <cstdint>

namespace fz::authentication
{

struct error
{
	enum type: std::uint8_t {
		none,
		user_disabled,
		user_nonexisting,
		ip_disallowed,
		invalid_credentials,
		auth_method_not_supported,

		/* Not user's fault */
		user_quota_reached,

		/* Internal errors */
		internal
	};

	constexpr error(type v = none) noexcept
		: v_(v)
	{}

	constexpr explicit operator bool() const noexcept
	{
		return v_ != none;
	}

	constexpr operator type() const noexcept
	{
		return v_;
	}

	constexpr bool is_internal() const noexcept
	{
		return v_ >= internal;
	}

	constexpr bool is_user_fault() const noexcept
	{
		return none < v_ && v_ < user_quota_reached;
	}

	template <typename String>
	friend String toString(const error &e)
	{
		using C = typename String::value_type;

		switch (e.v_) {
			case none: return fzS(C, "No error");
			case user_quota_reached: return fzS(C, "User quota reached");
			case user_disabled: return fzS(C, "User is disabled");
			case user_nonexisting: return fzS(C, "User does not exist");
			case ip_disallowed: return fzS(C, "IP is not allowed");
			case auth_method_not_supported: return fzS(C, "Auth method is not supported");
			case invalid_credentials: return fzS(C, "Invalid credentials");
			case internal: return fzS(C, "Internal error");
		}

		return fzS(C, "Unknown error");
	}

private:
	type v_;
};

}


#endif // FZ_AUTHENTICATION_ERROR_HPP
