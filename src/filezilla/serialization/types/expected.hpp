#ifndef FZ_SERIALIZATION_TYPES_EXPECTED_HPP
#define FZ_SERIALIZATION_TYPES_EXPECTED_HPP

#include "../../expected.hpp"
#include "../helpers.hpp"

namespace fz {

template <typename Archive, typename U>
void serialize(Archive &ar, unexpected<U> &u)
{
	ar(serialization::nvp(u.value_, "unexpected"));
}

}

#endif // FZ_SERIALIZATION_TYPES_EXPECTED_HPP
