#ifndef FZ_HTTP_FIELD_HPP
#define FZ_HTTP_FIELD_HPP

#include <string>
#include <optional>

#include <libfilezilla/string.hpp>

#include "../transformed_view.hpp"

namespace fz::http::field {

namespace detail {
struct component_tag {};
}

class component_view: public detail::component_tag
{
public:
	component_view() = default;

	template <typename T, std::enable_if_t<std::is_constructible_v<std::string_view, T>>* = nullptr>
	component_view(const T &rhs)
		: s_(std::string_view(rhs))
	{}

	operator std::string_view () const &
	{
		return s_;
	}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	bool empty() const
	{
		return s_.empty();
	}

	std::string_view str() const
	{
		return s_;
	}

protected:
	std::string_view s_;
};


class component: public detail::component_tag
{
public:
	component() = default;

	template <typename T, std::enable_if_t<std::is_constructible_v<std::string, T>>* = nullptr>
	component(T && rhs)
		: s_(std::forward<T>(rhs))
	{}

	operator std::string &&() &&
	{
		return std::move(s_);
	}

	operator std::string_view () const &
	{
		return s_;
	}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	bool empty() const
	{
		return s_.empty();
	}

	const std::string &str() const
	{
		return s_;
	}

protected:
	std::string s_;
};

template <typename Lhs, typename Rhs>
std::enable_if_t<
	(std::is_base_of_v<detail::component_tag, Lhs> && std::is_constructible_v<std::string_view, Rhs>) ||
	(std::is_base_of_v<detail::component_tag, Rhs> && std::is_constructible_v<std::string_view, Lhs>),
bool>
operator==(const Lhs &lhs, const Rhs &rhs)
{
	return fz::equal_insensitive_ascii(std::string_view(lhs), std::string_view(rhs));
}

template <typename Lhs, typename Rhs>
std::enable_if_t<
	(std::is_base_of_v<detail::component_tag, Lhs> && std::is_constructible_v<std::string_view, Rhs>) ||
	(std::is_base_of_v<detail::component_tag, Rhs> && std::is_constructible_v<std::string_view, Lhs>),
bool>
operator!=(const Lhs &lhs, const Rhs &rhs)
{
	return !fz::equal_insensitive_ascii(std::string_view(lhs), std::string_view(rhs));
}

template <typename Lhs, typename Rhs>
std::enable_if_t<
	(std::is_base_of_v<detail::component_tag, Lhs> && std::is_constructible_v<std::string_view, Rhs>) ||
	(std::is_base_of_v<detail::component_tag, Rhs> && std::is_constructible_v<std::string_view, Lhs>),
bool>
operator<(const Lhs &lhs, const Rhs &rhs)
{
	return fz::less_insensitive_ascii()(std::string_view(lhs), std::string_view(rhs));
}

struct name: component
{
	using component::component;
};

struct name_view: component_view
{
	using component_view::component_view;
};

struct value_view;

struct value: component
{
	using component::component;

	struct list;
	struct params_list;

	list as_list();
	value &append(value_view v);
	value &operator+=(value_view v);

	params_list as_params_list(bool comma = false);
	value &append_param(std::string_view p);
	value &operator%=(std::string_view p);

	// Equality check, disregarding any parameters
	bool is(component_view v) const;

	// Gets a value's parameter value with the given key.
	// Returns std::nullopt if the parameter doesn't exist
	std::optional<component_view> get_param(component_view key, bool case_insensitive = true) const;
};

struct value_view: component_view
{
	using component_view::component_view;

	struct list;
	struct params_list;

	list as_list() const;
	params_list as_params_list(bool comma = false) const;

	// Equality check, disregarding any parameters
	bool is(component_view v) const;

	// Gets a value's parameter value with the given key.
	// Returns std::nullopt if the parameter doesn't exist
	std::optional<component_view> get_param(component_view key, bool case_insensitive = true) const;
};

struct value::list
{
	list(value &v)
		: s_(v.s_)
	{}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	list &append(value_view v);

	value_view get(component_view v);
	value_view last();

	operator const std::string &() const
	{
		return s_;
	}

	auto iterable() const {
		return transformed_view(strtokenizer(s_, ",", false), [](auto s) {
			trim(s, " \t");
			return value_view(s);
		});
	}

private:
	std::string &s_;
};

struct value::params_list
{
	params_list(value &v, bool comma)
		: s_(v.s_)
		, sep_(comma ? "," : ";")
	{}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	params_list &append(value_view v);

	std::optional<component_view> get(component_view v, bool case_insensitive = true) const;

	operator const std::string &() const
	{
		return s_;
	}

	auto iterable() const {
		return transformed_view(strtokenizer(s_, sep_, false), [](auto s) {
			trim(s, " \t");
			return component_view(s);
		});
	}

private:
	std::string &s_;
	std::string_view sep_;
};

struct value_view::list
{
	list(value_view v)
		: s_(v.s_)
	{}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	value_view get(component_view v) const;
	value_view last() const;

	operator std::string_view () const
	{
		return s_;
	}

	auto iterable() const {
		return transformed_view(strtokenizer(s_, ",", false), [](auto s) {
			trim(s, " \t");
			return value_view(s);
		});
	}

private:
	std::string_view s_;
};

struct value_view::params_list
{
	params_list(value_view v, bool comma)
		: s_(v.s_)
		, sep_(comma ? "," : ";")
	{}

	explicit operator bool() const
	{
		return !s_.empty();
	}

	std::optional<component_view> get(component_view v, bool case_insensitive = true) const;

	operator std::string_view () const
	{
		return s_;
	}

	auto iterable() const {
		return transformed_view(strtokenizer(s_, sep_, false), [](auto s) {
			trim(s, " \t");
			return component_view(s);
		});
	}

private:
	std::string_view s_;
	std::string_view sep_;
};

}

namespace std {

template <>
struct common_type<std::string_view, fz::http::field::value_view> {
	using type = fz::http::field::value_view;
};

template <>
struct common_type<fz::http::field::value_view, std::string_view> {
	using type = fz::http::field::value_view;
};

}

#endif // FZ_HTTP_FIELD_HPP
