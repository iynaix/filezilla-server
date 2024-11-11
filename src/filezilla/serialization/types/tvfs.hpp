#ifndef FZ_SERIALIZATION_TYPES_TVFS_HPP
#define FZ_SERIALIZATION_TYPES_TVFS_HPP

#include "../../tvfs/mount.hpp"
#include "../../tvfs/limits.hpp"

#include "../helpers.hpp"
#include "../../string.hpp"
#include "../../build_info.hpp"

namespace fz::serialization {

template <typename Archive>
void serialize(Archive &ar, tvfs::mount_point &mp) {
	using namespace fz::serialization;

	if constexpr (trait::is_output_v<Archive> && trait::is_textual_v<Archive>) {
		if (!mp.old_native_path.empty()) {
			// Verify that the old and new path are still equivalent
			if (mp.native_path != tvfs::placeholders::convert_old_style_to_new(mp.old_native_path))
				mp.old_native_path.clear();
		}
	}

	ar.attributes(
		nvp(mp.tvfs_path, "tvfs_path"),
		nvp(mp.access, "access")
	)
	// See the comment in mount.hpp
	.optional_attribute(mp.old_native_path, "native_path")
	.optional_attribute(mp.native_path, "new_native_path")
	.optional_attribute(mp.recursive, "recursive")
	.optional_attribute(mp.flags, "flags");

	if constexpr (trait::is_input_v<Archive> && trait::is_textual_v<Archive>) {
		if (ar) {
			if (mp.native_path.empty() && !mp.old_native_path.empty())
				mp.native_path = tvfs::placeholders::convert_old_style_to_new(mp.old_native_path);
		}
	}
}

template <typename Archive>
void serialize(Archive &ar, tvfs::open_limits &l) {
	using fz::serialization::nvp;

	ar.optional_attribute(with_unlimited{l.files, tvfs::open_limits::unlimited}, "files")
	  .optional_attribute(with_unlimited{l.directories, tvfs::open_limits::unlimited}, "directories");
}

}

#endif // FZ_SERIALIZATION_TYPES_TVFS_HPP
