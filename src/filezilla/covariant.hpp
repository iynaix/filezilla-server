#ifndef FZ_COVARIANT_HPP
#define FZ_COVARIANT_HPP

#include <variant>

namespace fz {

/// \brief a variant type whose first member must be the base type of all the others
template <typename Base, typename... Ds>
struct covariant: std::variant<Base, Ds...>
{
	static_assert((std::is_base_of_v<Base, Ds> && ...), "The first type must be the base class of all the others");

	using std::variant<Base, Ds...>::variant;
};

}

#endif // COVARIANT_HPP
