#ifndef FZ_RMP_VERSION_HPP
#define FZ_RMP_VERSION_HPP

#include <climits>

#include "../serialization/trait.hpp"

namespace fz::rmp {

using serialization::version_t;

// General RMP version. Increase this any time the ABI changes in a way that is incompatible with the previous one.
inline constexpr version_t version { 3 };

}

#endif // FZ_RMP_VERSION_HPP
