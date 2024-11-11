#ifndef FZ_EXPECTED_HPP
#define FZ_EXPECTED_HPP

#include <variant>
#include <string>

namespace fz
{

template <typename T, typename E>
struct expected;

template <typename T>
struct unexpected
{
	constexpr unexpected() noexcept (std::is_nothrow_constructible_v<T>)
		: value_()
	{}

	constexpr explicit unexpected(T && v) noexcept(std::is_nothrow_constructible_v<T, T>)
		: value_(std::forward<T>(v))
	{}

	template <typename U, std::enable_if_t<std::is_constructible_v<T, U>>* = nullptr>
	constexpr unexpected(const unexpected<U> &v) noexcept(std::is_nothrow_constructible_v<T, U>)
		: value_(v.value_)
	{}

	template <typename U, std::enable_if_t<std::is_constructible_v<T, U>>* = nullptr>
	constexpr unexpected(unexpected<U> &&v) noexcept(std::is_nothrow_constructible_v<T, U>)
		: value_(std::forward<U>(std::move(v).value_))
	{}

	template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>>* = nullptr>
	constexpr unexpected(std::in_place_t, Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
		: value_(std::forward<Args>(args)...)
	{}

private:
	template <typename Archive, typename U>
	friend void serialize(Archive &ar, unexpected<U> &u);

	template <typename U, typename E>
	friend struct expected;

	template <typename U>
	friend struct unexpected;

	T value_;
};

template <typename T>
unexpected(T && v) -> unexpected<T>;

struct unexpect_t {
	explicit unexpect_t() = default;
};

inline constexpr unexpect_t unexpect{};

template <typename T, typename E>
struct expected: std::variant<T, unexpected<E>>
{
	using value_type = T;
	using error_type = E;
	using unexpected_type = unexpected<E>;

	using variant_type = std::variant<T, unexpected<E>>;

	using variant_type::variant_type;

	template <typename... Args, std::enable_if_t<std::is_constructible_v<variant_type, std::in_place_type_t<unexpected_type>, std::in_place_t, Args...>>* = nullptr>
	constexpr expected(unexpect_t, Args &&... args)
		: variant_type(std::in_place_type<unexpected_type>, std::in_place, std::forward<Args>(args)...)
	{}

	template <typename... Args, std::enable_if_t<std::is_constructible_v<variant_type, std::in_place_type_t<T>, Args...>>* = nullptr>
	constexpr expected(std::in_place_t, Args &&... args)
		: variant_type(std::in_place_type<T>, std::forward<Args>(args)...)
	{}

	constexpr bool has_value() const
	{
		return std::holds_alternative<T>(*this);
	}

	constexpr const T &value() const &
	{
		return *std::get_if<T>(this);
	}

	constexpr T &value() &
	{
		return *std::get_if<T>(this);
	}

	constexpr T &&value() &&
	{
		return std::move(*std::get_if<T>(this));
	}

	constexpr const T &operator*() const &
	{
		return *std::get_if<T>(this);
	}

	constexpr T &operator*() &
	{
		return *std::get_if<T>(this);
	}

	constexpr T &&operator*() &&
	{
		return std::move(*std::get_if<T>(this));
	}

	constexpr const T *operator->() const
	{
		return std::get_if<T>(this);
	}

	constexpr T *operator->()
	{
		return std::get_if<T>(this);
	}

	constexpr const E& error() const &
	{
		return std::get_if<unexpected<E>>(this)->value_;
	}

	constexpr E& error() &
	{
		return std::get_if<unexpected<E>>(this)->value_;
	}

	constexpr E&& error() &&
	{
		return std::move(std::get_if<unexpected<E>>(this)->value_);
	}

	constexpr explicit operator bool() const
	{
		return has_value();
	}

	template <typename U>
	constexpr T value_or(U && def) const&
	{
		return bool(*this) ? **this : static_cast<T>(std::forward<U>(def));
	}

	template <typename U>
	constexpr T value_or(U && def) &&
	{
		return bool(*this) ? std::move(**this) : static_cast<T>(std::forward<U>(def));
	}
};

}

#endif // FZ_EXPECTED_HPP
