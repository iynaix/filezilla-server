#ifndef FZ_UTIL_FILESYSTEM_HPP
#define FZ_UTIL_FILESYSTEM_HPP

#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/file.hpp>
#include <libfilezilla/logger.hpp>
#include <libfilezilla/string.hpp>
#include <libfilezilla/forward_like.hpp>

namespace fz::util::fs {

enum path_format: std::uint8_t {
	unix_format,
	windows_format,

#if defined(FZ_WINDOWS) && FZ_WINDOWS
	native_format = windows_format
#elif (defined(FZ_UNIX) && FZ_UNIX) || (defined(FZ_MAC) && FZ_MAC)
	native_format = unix_format
#else
#   error "Your platform is not supported"
#endif
};

enum path_kind: std::uint8_t {
	absolute_kind,
	relative_kind,
	any_kind
};

enum path_ownership: std::uint8_t {
	user_ownership,
	user_or_admin_ownership
};

//! Encapsulates a filesystem path.
//! Automatically converts to the underlying string type.
template <typename Char, path_format Format = native_format, path_kind Kind = any_kind>
class basic_path;

template <typename Char, path_format Format = native_format, path_kind Kind = any_kind>
class basic_path_list;

namespace detail
{
	template <typename T>
	struct is_basic_path: std::false_type{};

	template <typename Char, path_format Format, path_kind Kind>
	struct is_basic_path<basic_path<Char, Format, Kind>>: std::true_type
	{
		using char_type = Char;
		static constexpr path_format format_value = Format;
		static constexpr path_kind kind_value = Kind;
	};

	template <typename T>
	struct is_native_basic_path: std::false_type{};

	template <path_kind Kind, typename Char>
	struct is_native_basic_path<basic_path<Char, native_format, Kind>>: std::true_type{};

	template <typename T>
	struct is_basic_path_list: std::false_type{};

	template <typename Char, path_format Format, path_kind Kind>
	struct is_basic_path_list<basic_path_list<Char, Format, Kind>>: std::true_type{};

	struct no_validation_tag{};
	inline constexpr no_validation_tag no_validation;
}

template <typename Char, path_format Format>
struct basic_path_util
{
	using View = std::basic_string_view<Char>;
	using String = std::basic_string<Char>;

	static constexpr bool is_separator(Char ch)
	{
		if constexpr (Format == windows_format) {
			if (ch == Char('\\')) {
				return true;
			}
		}

		return ch == Char('/');
	}

	static constexpr View separator()
	{
		if constexpr (Format == windows_format) {
			return View(fzS(Char, "\\"));
		}
		else {
			return View(fzS(Char, "/"));
		}
	}

	static bool is_absolute(View string);
};

/*
 * Appending one path to another results in a path with potentially different kind.
 *
 * For all intent and purposes, if Rhs is a String type, then it's treated the same as if it were a non-validated-yet basic_path of the any_kind.
 *
 * Here's an explanatory table for the operator/():
 *
 * Relative / Absolute -> Absolute (rhs)
 * Relative / Relative -> Relative (lhs / rhs)
 * Relative / Any      -> Relative (lhs / rhs) if rhs is relative OR Relative (invalid) if rhs is absolute
 *
 * Absolute / Absolute -> Absolute (rhs)
 * Absolute / Relative -> Absolute (lhs / rhs)
 * Absolute / Any      -> Absolute (lhs / rhs) if rhs is relative OR Absolute (rhs) if rhs is absolute
 *
 * Any / Absolute      -> Absolute (rhs)
 * Any / Relative      -> Any      (lhs / rhs)
 * Any / Any           -> Any      (lhs / rhs) if rhs is relative OR Any (rhs) if rhs is absolute
 *
 * Hence:
 *
 * Lhs / Absolute -> Absolute (rhs)
 * Lhs / Relative -> Lhs      (lhs / rhs)
 * Lhs / Any      -> Lhs      (lhs / rhs) if rhs is relative OR {
 *							      (invalid) if Lhs is Relative,
 *                                (rhs) in the other cases
 *                            } if rhs is absolute
 *
 * Here's an explanatory table for the operator/=():
 *
 * Relative /= Absolute -> NA
 * Relative /= Relative -> (lhs / rhs)
 * Relative /= Any      -> (lhs / rhs) if rhs is relative OR (invalid) if rhs is absolute
 *
 * Absolute /= Absolute -> (rhs)
 * Absolute /= Relative -> (lhs / rhs)
 * Absolute /= Any      -> (lhs / rhs) if rhs is relative OR (rhs) if rhs is absolute
 *
 * Any /= Absolute      -> (rhs)
 * Any /= Relative      -> (lhs / rhs)
 * Any /= Any           -> (lhs / rhs) if rhs is relative OR (rhs) if rhs is absolute
 *
 * Here's an explanatory table for the operator=() and copy constructor:
 *
 * Relative = Absolute -> NA
 * Relative = Relative -> (rhs) // This is the default
 * Relative = Any      -> (rhs) if rhs is relative OR (invalid) if rhs is absolute
 *
 * Absolute = Absolute -> (rhs) // This is the default
 * Absolute = Relative -> NA
 * Absolute = Any      -> (rhs) if rhs is absolute OR (invalid) if rhs is relative
 *
 * Any = Absolute      -> (rhs)
 * Any = Relative      -> (rhs)
 * Any = Any           -> (rhs) // This is the default
 *
 */

template <typename Char, path_format Format, path_kind Kind>
class basic_path {
	using View = std::basic_string_view<Char>;
	using String = std::basic_string<Char>;
	using Util = basic_path_util<Char, Format>;

	static_assert(Format == unix_format || Format == windows_format, "path_format not recognized");
	static_assert(Kind == absolute_kind || Kind == relative_kind || Kind == any_kind, "path_kind not recognized");

	void validate(path_format invalid_chars_within_path_elements_as_in_path_format = native_format);
	void normalize();

	template <auto F = Format, std::enable_if_t<F == unix_format>* = nullptr>
	std::size_t first_after_root() const;

	template <auto F = Format, std::enable_if_t<F == windows_format>* = nullptr>
	std::size_t first_after_root(std::basic_string_view<Char> *pspecifier = nullptr, bool *pis_unc = nullptr) const;

	template <typename, path_format, path_kind>
	friend class basic_path;

	template <typename S, std::enable_if_t<!detail::is_basic_path<std::decay_t<S>>::value && std::is_constructible_v<String, S>>* = nullptr>
	basic_path(S && s, detail::no_validation_tag)
		: string_(std::forward<S>(s))
	{
	}

	String string_;

public:
	using char_type = Char;
	static inline constexpr path_format format_value = Format;
	static inline constexpr path_kind kind_value = Kind;

	constexpr auto static separator()
	{
		return Util::separator()[0];
	}

	constexpr bool static is_separator(Char c)
	{
		return Util::is_separator(c);
	}

	basic_path()
	{}

	basic_path(String && string, path_format invalid_chars_within_path_elements_as_in_path_format = native_format);

	template <typename S, std::enable_if_t<!detail::is_basic_path<std::decay_t<S>>::value && std::is_constructible_v<String, S>>* = nullptr>
	basic_path(S && s, path_format invalid_chars_within_path_elements_as_in_path_format = native_format)
		: basic_path(String(std::forward<S>(s)), invalid_chars_within_path_elements_as_in_path_format)
	{
		normalize();
		validate(invalid_chars_within_path_elements_as_in_path_format);
	}

	template <typename Rhs, std::enable_if_t<
		detail::is_basic_path<std::decay_t<Rhs>>::value>* = nullptr>
	basic_path(Rhs && rhs)
	{
		constexpr auto rhs_kind = std::decay_t<Rhs>::kind_value;

		if constexpr (Kind == relative_kind && rhs_kind == absolute_kind)
		{
			static_assert(rhs_kind != absolute_kind, "A basic_path of absolute_kind cannot be copied/moved onto a basic_path of relative_kind");
		}
		if constexpr (Kind == absolute_kind && rhs_kind == relative_kind)
		{
			static_assert(rhs_kind != relative_kind, "A basic_path of relative_kind cannot be copied/moved onto a basic_path of absolute_kind");
		}
		else
		{
			if constexpr (Kind != any_kind) {
				if (rhs.is_absolute() != (Kind == absolute_kind)) {
					string_ = {};
					return;
				}
			}

			string_ = forward_like<Rhs>(rhs.string_);
		}
	}

	template <typename Rhs, std::enable_if_t<
		detail::is_basic_path<std::decay_t<Rhs>>::value>* = nullptr>
	basic_path &operator/=(Rhs && rhs) &
	{
		constexpr auto rhs_kind = std::decay_t<Rhs>::kind_value;

		if constexpr (Kind == relative_kind && rhs_kind == absolute_kind) {
			static_assert(rhs_kind != absolute_kind, "A basic_path of absolute_kind cannot be appended to a path of relative_kind");
		}
		else
		if (!string_.empty()) {
			if (rhs.is_absolute()) {
				if constexpr (Kind == relative_kind) {
					string_ = {};
				}
				else {
					string_ = forward_like<Rhs>(rhs.string_);
				}
			}
			else
			if (!rhs.string_.empty()) {
				string_.append(Util::separator()).append(rhs.string_);
				normalize();
			}
			else {
				string_ = {};
			}
		}

		return *this;
	}

	template <typename Rhs, std::enable_if_t<
		detail::is_basic_path<std::decay_t<Rhs>>::value>* = nullptr>
	basic_path operator/=(Rhs && rhs) &&
	{
		return std::move(*this /= std::forward<Rhs>(rhs));
	}

	template <typename Rhs, std::enable_if_t<
		!detail::is_basic_path<std::decay_t<Rhs>>::value &&
		std::is_constructible_v<String, Rhs>>* = nullptr>
	basic_path &operator/=(Rhs && rhs) &
	{
		return *this /= basic_path<Char, Format, any_kind>(std::forward<Rhs>(rhs));
	}

	template <typename Rhs, std::enable_if_t<
		!detail::is_basic_path<std::decay_t<Rhs>>::value &&
		std::is_constructible_v<String, Rhs>>* = nullptr>
	basic_path operator/=(Rhs && rhs) &&
	{
		return std::move(*this /= std::forward<Rhs>(rhs));
	}

	//! Generic operator= where Rhs is another kind of path. It relies on the heterogeneous copy constructors.
	template <typename Rhs, std::enable_if_t<
		detail::is_basic_path<std::decay_t<Rhs>>::value>* = nullptr>
	basic_path &operator=(Rhs &&rhs)
	{
		basic_path copy(std::forward<Rhs>(rhs));

		swap(*this, copy);

		return *this;
	}

	friend void swap(basic_path &lhs, basic_path &rhs) noexcept(noexcept(std::declval<String>().swap(std::declval<String&>())))
	{
		lhs.string_.swap(rhs.string_);
	}

	/*****/

	explicit operator bool() const { return !string_.empty(); }
	bool is_valid() const { return !string_.empty(); }

	operator const String &() const & { return string_; }
	operator String () && { return std::move(string_); }
	operator View () const { return string_; }

	const String &str() const & { return string_; }
	String str() && { return std::move(string_); }
	View str_view() const { return string_; }

	const Char *c_str() const { return string_.c_str(); }

	//! Returns a vector with the elements of the path, following the root
	//! The returned vector's lifetime must be shorter than the one of the path itself.
	std::vector<View> elements_view() const;

	//! Returns a vector with the elements of the path, following the root
	std::vector<String> elements() const;

	//! Treats the path as referring to a file and tries to open it.
	fz::file open(fz::file::mode mode, file::creation_flags flags = file::existing) const;

	//! Treats the path as referring to a directory and tries to create it.
	result mkdir(bool recurse, mkdir_permissions permissions = mkdir_permissions::normal, native_string * last_created = nullptr) const;

	//! \brief Checks whether the ownership of each segment of the path is compatible with the given ownership
	//! \returns true if the checks succeeds, false otherwise.
	template <typename C = Char, path_format F = Format, std::enable_if_t<std::is_same_v<C, native_string::value_type> && F == native_format>* = nullptr>
	bool check_ownership(path_ownership ownership, logger_interface &logger = get_null_logger()) const;

	//! Returns the type of the object referred by the path, optionally following symlinks.
	//! If the object doesn't exist, local_filesys::type::unknown is returned.
	local_filesys::type type(bool follow_links = false) const;

	//! Returns the last element of the path, optionally without the suffixes.
	basic_path<Char, Format, relative_kind> base(bool remove_suffixes = false) const;

	//! Returns the parent path
	basic_path parent() const &;
	basic_path parent() &&;

	//! Makes the path into its own parent
	basic_path &make_parent();

	//! Returns whether it's an absolute path
	bool is_absolute() const;

	//! Returns whether it's a base
	bool is_base() const;

	//! Treats the path as referring to a file and tries to open it, then closes it.
	//! If successful, the path is returned as-is, otherwise an invalid path is returned.
	//! If mode == writing, then the file gets created, if not existing already in that path.
	basic_path if_openable(fz::file::mode mode, file::creation_flags flags = {}) &&;
	basic_path if_openable(fz::file::mode mode, file::creation_flags flags = {}) const &
	{
		return basic_path(*this).if_openable(mode, flags);
	}

	//! If the path is a base, then returns the path as-is, otherwise an invalid path is returned.
	//! \note: an absolute path can never be a base.
	basic_path if_base() &&;
	basic_path if_base() const &
	{
		return basic_path(*this).if_base();
	}

	template <typename Path>
	basic_path resolve(const Path &path, fz::file::mode mode, file::creation_flags flags = {}) &&
	{
		return basic_path(std::move(*this) /= path).if_openable(mode, flags);
	}

	template <typename Path>
	basic_path resolve(const Path &path, fz::file::mode mode, file::creation_flags flags = {}) const &
	{
		return basic_path(*this / path).if_openable(mode, flags);
	}
};

// Lhs / Absolute -> Absolute (rhs)
template <typename Lhs, typename Rhs, std::enable_if_t<
	detail::is_basic_path<std::decay_t<Lhs>>::value &&
	detail::is_basic_path<std::decay_t<Rhs>>::kind_value == absolute_kind>* = nullptr>
decltype(auto) operator/(Lhs &&, Rhs &&rhs)
{
	return std::forward<Rhs>(rhs);
}

// Lhs / Relative -> Lhs (lhs / rhs)
// Lhs / Any      -> Lhs (lhs / rhs) if rhs is relative OR {
//                                       (invalid) if Lhs is Relative,
//                                       (rhs) in the other cases
//                                   } if rhs is absolute
template <typename Lhs, typename Rhs, std::enable_if_t<
	detail::is_basic_path<std::decay_t<Lhs>>::value &&
	detail::is_basic_path<std::decay_t<Rhs>>::kind_value != absolute_kind>* = nullptr>
std::decay_t<Lhs> operator/(Lhs &&lhs, Rhs &&rhs)
{
	return std::decay_t<Lhs>(std::forward<Lhs>(lhs)) /= std::forward<Rhs>(rhs);
}

// Lhs / String -> Lhs (lhs / rhs) if rhs is relative OR {
//                                     (invalid) if Lhs is Relative,
//                                     (rhs) in the other cases
//                                 } if rhs is absolute
template <typename Lhs, typename Rhs, std::enable_if_t<
	detail::is_basic_path<std::decay_t<Lhs>>::value &&
	!detail::is_basic_path<std::decay_t<Rhs>>::value &&
	std::is_constructible_v<std::basic_string<typename std::decay_t<Lhs>::char_type>, Rhs>>* = nullptr>
std::decay_t<Lhs> operator/(Lhs &&lhs, Rhs &&rhs)
{
	return std::decay_t<Lhs>(std::forward<Lhs>(lhs)) /= std::forward<Rhs>(rhs);
}

template <typename Char, path_format Format, path_kind Kind>
bool operator==(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() == rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
bool operator!=(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() != rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
bool operator<(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() < rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
bool operator>(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() > rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
bool operator<=(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() <= rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
bool operator>=(const basic_path<Char, Format, Kind> &lhs, const basic_path<Char, Format, Kind> &rhs)
{
	return lhs.str() >= rhs.str();
}

template <typename Char, path_format Format, path_kind Kind>
class basic_path_list: public std::vector<basic_path<Char, Format, Kind>> {
	using Path = basic_path<Char, Format, Kind>;

public:
	using std::vector<Path>::vector;

	template <typename... P, std::enable_if_t<!(detail::is_basic_path_list<std::decay_t<P>>::value || ...) && (std::is_constructible_v<Path, P> && ...)>* = nullptr>
	basic_path_list(P &&... p)
		: std::vector<Path>{Path(std::forward<P>(p))...}
	{}

	//! Until successful, appends the other, relative, path to each path in the list and treats the resulting path as referring to a file, then tries to open it and subsequently closes it.
	//! If successful, the resulting path is returned as-is, otherwise an invalid path is returned.
	//! If mode == writing, then the file gets created in the first usable directory path, if not existing already in that path.
	template <typename ToResolve>
	Path resolve(const ToResolve &other, fz::file::mode mode, file::creation_flags flags = {}) const
	{
		for (const auto &p: *this) {
			if (auto res = (p/other).if_openable(mode, flags))
				return res;
		}

		return {};
	}

	basic_path_list &operator+=(basic_path_list rhs);

	template <typename Rhs>
	basic_path_list &operator/=(const Rhs &rhs)
	{
		for (auto &p: *this)
			p /= rhs;

		return *this;
	}

	basic_path_list operator+(basic_path_list rhs) const &;
	basic_path_list operator+(basic_path_list rhs) &&;

	template <typename Rhs>
	basic_path_list operator/(const Rhs &rhs) const &
	{
		return basic_path_list(*this) /= rhs;
	}

	template <typename Rhs>
	basic_path_list operator/(const Rhs &rhs) &&
	{
		return std::move(*this /= rhs);
	}
};

template <typename Char, path_format Format, path_kind Kind>
basic_path_list<Char, Format, Kind> operator+(basic_path<Char, Format, Kind> lhs, basic_path_list<Char, Format, Kind> rhs) {
	return basic_path_list<Char, Format, Kind>{std::move(lhs)} += std::move(rhs);
}

template <typename Char, path_format Format, path_kind Kind>
basic_path_list<Char, Format, Kind> operator+(basic_path<Char, Format, Kind> lhs, basic_path<Char, Format, Kind> rhs)
{
	return { std::move(lhs), std::move(rhs) };
}

using path = basic_path<std::string::value_type, native_format, any_kind>;
using path_list = basic_path_list<std::string::value_type, native_format, any_kind>;
using wpath = basic_path<std::wstring::value_type, native_format, any_kind>;
using wpath_list = basic_path_list<std::wstring::value_type, native_format, any_kind>;
using native_path = basic_path<native_string::value_type, native_format, any_kind>;
using native_path_list = basic_path_list<native_string::value_type, native_format, any_kind>;

using unix_path = basic_path<std::string::value_type, unix_format, any_kind>;
using unix_path_list = basic_path_list<std::string::value_type, unix_format, any_kind>;
using unix_wpath = basic_path<std::wstring::value_type, unix_format, any_kind>;
using unix_wpath_list = basic_path_list<std::wstring::value_type, unix_format, any_kind>;
using unix_native_path = basic_path<native_string::value_type, unix_format, any_kind>;
using unix_native_path_list = basic_path_list<native_string::value_type, unix_format, any_kind>;

using windows_path = basic_path<std::string::value_type, windows_format, any_kind>;
using windows_path_list = basic_path_list<std::string::value_type, windows_format, any_kind>;
using windows_wpath = basic_path<std::wstring::value_type, windows_format, any_kind>;
using windows_wpath_list = basic_path_list<std::wstring::value_type, windows_format, any_kind>;
using windows_native_path = basic_path<native_string::value_type, windows_format, any_kind>;
using windows_native_path_list = basic_path_list<native_string::value_type, windows_format, any_kind>;

using absolute_path = basic_path<std::string::value_type, native_format, absolute_kind>;
using absolute_path_list = basic_path_list<std::string::value_type, native_format, absolute_kind>;
using absolute_wpath = basic_path<std::wstring::value_type, native_format, absolute_kind>;
using absolute_wpath_list = basic_path_list<std::wstring::value_type, native_format, absolute_kind>;
using absolute_native_path = basic_path<native_string::value_type, native_format, absolute_kind>;
using absolute_native_path_list = basic_path_list<native_string::value_type, native_format, absolute_kind>;

using absolute_unix_path = basic_path<std::string::value_type, unix_format, absolute_kind>;
using absolute_unix_path_list = basic_path_list<std::string::value_type, unix_format, absolute_kind>;
using absolute_unix_wpath = basic_path<std::wstring::value_type, unix_format, absolute_kind>;
using absolute_unix_wpath_list = basic_path_list<std::wstring::value_type, unix_format, absolute_kind>;
using absolute_unix_native_path = basic_path<native_string::value_type, unix_format, absolute_kind>;
using absolute_unix_native_path_list = basic_path_list<native_string::value_type, unix_format, absolute_kind>;

using absolute_windows_path = basic_path<std::string::value_type, windows_format, absolute_kind>;
using absolute_windows_path_list = basic_path_list<std::string::value_type, windows_format, absolute_kind>;
using absolute_windows_wpath = basic_path<std::wstring::value_type, windows_format, absolute_kind>;
using absolute_windows_wpath_list = basic_path_list<std::wstring::value_type, windows_format, absolute_kind>;
using absolute_windows_native_path = basic_path<native_string::value_type, windows_format, absolute_kind>;
using absolute_windows_native_path_list = basic_path_list<native_string::value_type, windows_format, absolute_kind>;

using relative_path = basic_path<std::string::value_type, native_format, relative_kind>;
using relative_path_list = basic_path_list<std::string::value_type, native_format, relative_kind>;
using relative_wpath = basic_path<std::wstring::value_type, native_format, relative_kind>;
using relative_wpath_list = basic_path_list<std::wstring::value_type, native_format, relative_kind>;
using relative_native_path = basic_path<native_string::value_type, native_format, relative_kind>;
using relative_native_path_list = basic_path_list<native_string::value_type, native_format, relative_kind>;

using relative_unix_path = basic_path<std::string::value_type, unix_format, relative_kind>;
using relative_unix_path_list = basic_path_list<std::string::value_type, unix_format, relative_kind>;
using relative_unix_wpath = basic_path<std::wstring::value_type, unix_format, relative_kind>;
using relative_unix_wpath_list = basic_path_list<std::wstring::value_type, unix_format, relative_kind>;
using relative_unix_native_path = basic_path<native_string::value_type, unix_format, relative_kind>;
using relative_unix_native_path_list = basic_path_list<native_string::value_type, unix_format, relative_kind>;

using relative_windows_path = basic_path<std::string::value_type, windows_format, relative_kind>;
using relative_windows_path_list = basic_path_list<std::string::value_type, windows_format, relative_kind>;
using relative_windows_wpath = basic_path<std::wstring::value_type, windows_format, relative_kind>;
using relative_windows_wpath_list = basic_path_list<std::wstring::value_type, windows_format, relative_kind>;
using relative_windows_native_path = basic_path<native_string::value_type, windows_format, relative_kind>;
using relative_windows_native_path_list = basic_path_list<native_string::value_type, windows_format, relative_kind>;

extern template class basic_path<std::string::value_type, unix_format, any_kind>;
extern template class basic_path_list<std::string::value_type, unix_format, any_kind>;
extern template class basic_path<std::wstring::value_type, unix_format, any_kind>;
extern template class basic_path_list<std::wstring::value_type, unix_format, any_kind>;

extern template class basic_path<std::string::value_type, windows_format, any_kind>;
extern template class basic_path_list<std::string::value_type, windows_format, any_kind>;
extern template class basic_path<std::wstring::value_type, windows_format, any_kind>;
extern template class basic_path_list<std::wstring::value_type, windows_format, any_kind>;

extern template class basic_path<std::string::value_type, unix_format, absolute_kind>;
extern template class basic_path_list<std::string::value_type, unix_format, absolute_kind>;
extern template class basic_path<std::wstring::value_type, unix_format, absolute_kind>;
extern template class basic_path_list<std::wstring::value_type, unix_format, absolute_kind>;

extern template class basic_path<std::string::value_type, windows_format, absolute_kind>;
extern template class basic_path_list<std::string::value_type, windows_format, absolute_kind>;
extern template class basic_path<std::wstring::value_type, windows_format, absolute_kind>;
extern template class basic_path_list<std::wstring::value_type, windows_format, absolute_kind>;

extern template class basic_path<std::string::value_type, unix_format, relative_kind>;
extern template class basic_path_list<std::string::value_type, unix_format, relative_kind>;
extern template class basic_path<std::wstring::value_type, unix_format, relative_kind>;
extern template class basic_path_list<std::wstring::value_type, unix_format, relative_kind>;

extern template class basic_path<std::string::value_type, windows_format, relative_kind>;
extern template class basic_path_list<std::string::value_type, windows_format, relative_kind>;
extern template class basic_path<std::wstring::value_type, windows_format, relative_kind>;
extern template class basic_path_list<std::wstring::value_type, windows_format, relative_kind>;

extern template bool native_path::check_ownership(path_ownership, logger_interface &) const;
extern template bool absolute_native_path::check_ownership(path_ownership, logger_interface &) const;
extern template bool relative_native_path::check_ownership(path_ownership, logger_interface &) const;

template <typename T, std::enable_if_t<detail::is_native_basic_path<std::decay_t<T>>::value>* = nullptr>
struct only_dirs_t
{
	only_dirs_t(T && path)
		: path_(std::forward<T>(path))
	{}

private:
	template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>*>
	friend class native_directory_iterator;

	T path_;
};

template <typename T>
only_dirs_t(T && t) -> only_dirs_t<T>;

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
class native_directory_iterator
{
public:
	using iterator_category = std::input_iterator_tag;
	using difference_type   = std::ptrdiff_t;
	using value_type        = native_path;
	using pointer           = value_type*;
	using reference         = value_type&;

	struct sentinel{};

	native_directory_iterator(Dir && dir, bool iterate_over_dirs_only = false)
		: dir_(std::forward<Dir>(dir))
	{
		lfs_.begin_find_files(dir_, iterate_over_dirs_only);
		++*this;
	}

	native_directory_iterator(only_dirs_t<Dir> &&od)
		: native_directory_iterator(std::forward<Dir>(od.path_), true)
	{}

	~native_directory_iterator()
	{
		lfs_.end_find_files();
	}

	native_directory_iterator &operator++()
	{
		native_string name;
		is_valid_ = lfs_.get_next_file(name);
		curr_path_ = dir_ / name;

		return *this;
	}

	const value_type &operator*() const
	{
		return curr_path_;
	}

	bool operator!=(const sentinel &) const
	{
		return is_valid_;
	}

	bool operator==(const sentinel &) const
	{
		return !is_valid_;
	}

private:
	local_filesys lfs_;
	bool is_valid_;
	Dir dir_;
	value_type curr_path_{};
};

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
native_directory_iterator(Dir && dir, bool iterate_over_dirs_only = false) -> native_directory_iterator<Dir>;

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
native_directory_iterator(only_dirs_t<Dir> &&od) -> native_directory_iterator<Dir>;

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
native_directory_iterator<Dir> begin(Dir && root_path) { return { std::forward<Dir>(root_path) }; }

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
typename native_directory_iterator<Dir>::sentinel end(Dir &&) { return {}; }

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
only_dirs_t<Dir> only_dirs(Dir &&dir) { return { std::forward<Dir>(dir) }; }

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
native_directory_iterator<Dir> begin(only_dirs_t<Dir> root_path) { return { std::move(root_path) }; }

template <typename Dir, std::enable_if_t<detail::is_native_basic_path<std::decay_t<Dir>>::value>* = nullptr>
typename native_directory_iterator<Dir>::sentinel end(only_dirs_t<Dir>) { return {}; }

}

#endif // FZ_UTIL_FILESYSTEM_HPP
