#ifndef FZ_TVFS_PERMISSIONS_HPP
#define FZ_TVFS_PERMISSIONS_HPP

#include "../enum_bitops.hpp"

namespace fz::tvfs {

enum class permissions: unsigned char
{
	read                         = 1 << 0, ///< Can be read from
	write                        = 1 << 1, ///< Can be written to
	rename                       = 1 << 2, ///< Can be renamed
	remove                       = 1 << 3, ///< Can be deleted
	list_mounts                  = 1 << 4, ///< Only applicable if type == dir. Allows to list mountpoints even if normal files and subdirs cannot be listed because the read bit is not set.
	apply_recursively            = 1 << 5, ///< Only applicable if type == dir: applies permissions to subdirs too.
	allow_structure_modification = 1 << 6, ///< Only applicable if type == dir: allows to create/rename/delete subdirs.

	access_mask = read | write | rename | remove | list_mounts
};

FZ_ENUM_BITOPS_DEFINE_FOR(permissions)

}

#endif // FZ_TVFS_PERMISSIONS_HPP
