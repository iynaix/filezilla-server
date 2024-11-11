#ifndef FZ_TVFS_VALIDATION_HPP
#define FZ_TVFS_VALIDATION_HPP

#include <variant>
#include <vector>

#include <libfilezilla/string.hpp>
#include "../util/filesystem.hpp"

namespace fz::tvfs::validation
{

struct no_error{};
struct path_is_empty{};
struct path_is_not_absolute{};
struct path_has_invalid_characters{};
struct invalid_placeholder_values {
	std::vector<native_string> explanations;
};

struct result: std::variant<
	no_error,
	path_is_empty,
	path_is_not_absolute,
	path_has_invalid_characters,
	invalid_placeholder_values
>{
	using variant::variant;

	#ifdef FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER
	#	error "FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER already defined"
	#endif

	#define FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER(type) \
		const struct type *type() const                       \
		{                                                     \
			return std::get_if<struct type>(this);            \
		}                                                     \
		/***/


		FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER(path_is_empty)
		FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER(path_is_not_absolute)
		FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER(path_has_invalid_characters)
		FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER(invalid_placeholder_values)

	#undef FZ_TVFS_PLACEHOLDER_VALIDATION_ERROR_GETTER

	operator bool() const
	{
		return std::get_if<no_error>(this);
	}
};

result validate_tvfs_path(std::string_view path);
result validate_native_path(native_string_view path, util::fs::path_format path_format);

}

#endif // FZ_TVFS_VALIDATION_HPP
