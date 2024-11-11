#include "validation.hpp"
#include "placeholders.hpp"

namespace fz::tvfs::validation
{

template <typename CharT>
static result validate_path(std::basic_string_view<CharT> path, util::fs::path_format path_format)
{
	using namespace util::fs;
	using unix_path = basic_path<CharT, unix_format, any_kind>;
	using windows_path = basic_path<CharT, windows_format, any_kind>;

	auto check = [](const auto &path) -> result {
		if (!path.is_absolute()) {
			return path_is_not_absolute();
		}

		if (!path.is_valid()) {
			return path_has_invalid_characters();
		}

		if constexpr (std::is_same_v<char, CharT>) {
			// Path must be in utf8 when using single-byte encoding
			if (!fz::is_valid_utf8(path)) {
				return path_has_invalid_characters();
			}
		}

		return no_error();
	};

	if (path.empty()) {
		return path_is_empty();
	}

	if (path_format == unix_format) {
		return check(unix_path(path));
	}

	return check(windows_path(path));
}

result validate_tvfs_path(std::string_view path)
{
	return validate_path(path, util::fs::unix_format);
}

result validate_native_path(native_string_view path, util::fs::path_format path_format)
{
	// ** Placeholders validation
	std::size_t pos = 0;
	native_string explanation;

	validation::invalid_placeholder_values ipv;
	while (!(explanation = placeholders::next_invalid_value_explanation(path, pos)).empty())
		ipv.explanations.push_back(std::move(explanation));

	if (!ipv.explanations.empty())
		return ipv;

	return validate_path(path, path_format);
}

}
