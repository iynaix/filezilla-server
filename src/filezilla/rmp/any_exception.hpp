#ifndef FZ_RMP_ANY_EXCEPTION_HPP
#define FZ_RMP_ANY_EXCEPTION_HPP

#include <climits>

#include "../covariant.hpp"
#include "../serialization/types/variant.hpp"
#include "../util/overload.hpp"

#include "exceptions.hpp"

namespace fz::rmp {

struct any_exception: fz::covariant<
	exception::generic,
	exception::serialization_error,
	exception::message_not_implemented
> {
	using covariant::covariant;

	template <typename... Funcs>
	auto handle(Funcs &&... funcs) &&
	{
		return std::visit(util::overload{std::forward<Funcs>(funcs)...}, static_cast<covariant::variant &&>(std::move(*this)));
	}

	template <typename... Funcs>
	auto handle(Funcs &&... funcs) &
	{
		return std::visit(util::overload{std::forward<Funcs>(funcs)...}, static_cast<covariant::variant &>(*this));
	}

	template <typename... Funcs>
	auto handle(Funcs &&... funcs) const &
	{
		return std::visit(util::overload{std::forward<Funcs>(funcs)...}, static_cast<const covariant::variant &>(*this));
	}

	template <typename Archive>
	void serialize(Archive &ar)
	{
		ar(serialization::base_class<covariant>(this));

		// On the outer layer, in session.ipp, if we detect any serialization errors, we send out an exception.
		// This implies that, in case there'd be serialization error about an exception itself, then we'd get
		// ping-pong's between client and server about it.
		// To avoid this, we don't want to send out any exception if there's a serialization error about exceptions themselves.
		// For lack of (currently) a bettwer way, we signal this to the outer layer by negating any error we might have gotten here.
		// The outer layer will check whether the error is negative, if so, no exception is sent out and the connection is closed instead.
		ar.error(-ar.error());
	}
};

namespace trait {

	template <typename T>
	struct is_any_exception: std::is_base_of<any_exception, T>{};

	template <typename T>
	constexpr inline bool is_any_exception_v = is_any_exception<T>::value;

}

}

FZ_SERIALIZATION_SPECIALIZE_ALL(fz::rmp::any_exception, unversioned_member_serialize)

#endif // FZ_RMP_ANY_EXCEPTION_HPP
