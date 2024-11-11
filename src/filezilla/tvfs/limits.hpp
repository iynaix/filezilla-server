#ifndef FZ_TVFS_LIMITS_HPP
#define FZ_TVFS_LIMITS_HPP

#include <cstdint>

namespace fz::tvfs {

struct open_limits
{
	using type = std::uint16_t;
	static inline constexpr type unlimited = 0;

	type files{unlimited};
	type directories{unlimited};
};

}
#endif // FZ_TVFS_LIMITS_HPP
