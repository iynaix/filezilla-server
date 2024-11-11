#ifndef FZ_UTIL_INTEGRAL_OPS_HPP
#define FZ_UTIL_INTEGRAL_OPS_HPP

#include <cstdint>
#include <limits>
#include <cinttypes>
#include <algorithm>
#include <array>

#include <libfilezilla/buffer.hpp>

#include "parser.hpp"

namespace fz::util {

struct integral_op_result
{
	enum which
	{
		ok        = 0,
		bad_scale = 1,
		bad_base  = 2,
		bad_value = 3,
		overflow  = 4,
		underflow = 5
	};

	integral_op_result(which result)
		: result_(result)
	{}

	operator bool() const
	{
		return result_ == ok;
	}

	operator which() const
	{
		return result_;
	}

private:
	which result_;
};

template <typename T, std::enable_if_t<
	std::is_integral_v<T>
>* = nullptr>
integral_op_result increment(T &value, uintmax_t amount, uintmax_t scale = 1, std::decay_t<T> min = std::numeric_limits<T>::min(), std::decay_t<T> max = std::numeric_limits<T>::max())
{
	if (scale < 1)
		return integral_op_result::bad_scale;

	if (amount > std::numeric_limits<T>::max() / scale)
		return integral_op_result::overflow;

	amount *= scale;

	if (max < value || uintmax_t(max-value) < amount)
		return integral_op_result::overflow;

	if (min > value && uintmax_t(min-value) > amount)
		return integral_op_result::underflow;

	value += T(amount);

	return integral_op_result::ok;
}

template <typename T, std::enable_if_t<
	std::is_integral_v<T>
>* = nullptr>
integral_op_result decrement(T &value, uintmax_t amount, uintmax_t scale = 1, std::decay_t<T> min = std::numeric_limits<T>::min(), std::decay_t<T> max = std::numeric_limits<T>::max())
{
	if (scale < 1)
		return integral_op_result::bad_scale;

	if (amount > std::numeric_limits<T>::max() / scale)
		return integral_op_result::overflow;

	amount *= scale;

	if (max < value && uintmax_t(value-max) > amount)
		return integral_op_result::overflow;

	if (min > value || uintmax_t(value-min) < amount)
		return integral_op_result::underflow;

	value -= T(amount);

	return integral_op_result::ok;
}

template <typename String, typename T, std::enable_if_t<
	std::is_integral_v<T>
>* = nullptr>
auto convert(const String &s, T &value, uintmax_t scale = 1, std::decay_t<T> min = std::numeric_limits<T>::min(), std::decay_t<T> max = std::numeric_limits<T>::max())
	-> decltype(util::parseable_range(s), integral_op_result(integral_op_result::ok))
{
	if (scale < 1)
		return integral_op_result::bad_scale;

	util::parseable_range r(s);

	bool has_overflown = false;

	if (T new_value = 0; !(parse_int(r, new_value, 10, &has_overflown) && eol(r))) {
		return has_overflown
			? integral_op_result::overflow
			: integral_op_result::bad_value;
	}
	else {
		if (new_value < 0) {
			if (uintmax_t(~new_value)+1 > (uintmax_t(~std::numeric_limits<T>::min())+1) / scale)
				return integral_op_result::underflow;
		}
		else
		if (uintmax_t(new_value) > uintmax_t(std::numeric_limits<T>::max()) / scale)
			return integral_op_result::overflow;

		new_value *= T(scale);

		if (new_value < min)
			return integral_op_result::underflow;

		if (new_value > max)
			return integral_op_result::overflow;

		value = new_value;
	}

	return integral_op_result::ok;
}

namespace detail {

	template <typename T, typename = void>
	struct has_begin: std::false_type{};

	template <typename T>
	struct has_begin<T, std::void_t<decltype(std::begin(std::declval<T&>()))>>: std::true_type
	{
		using iterator_type = decltype(std::begin(std::declval<T&>()));
		using iterator_value_type = typename std::iterator_traits<iterator_type>::value_type;
	};

	template <typename T, typename = void>
	struct has_push_back: std::false_type{};

	template <typename T>
	struct has_push_back<T, std::void_t<decltype(std::declval<T&>().push_back(std::declval<has_begin<T>::iterator_value_type>()))>>: std::true_type {};

	template <typename T, typename = void>
	struct has_string_append: std::false_type{};

	template <typename T>
	struct has_string_append<T, std::void_t<
		decltype(std::declval<T&>().append(0, std::declval<typename has_begin<T>::iterator_value_type>())),
		decltype(std::declval<T&>().insert(std::declval<typename has_begin<T>::iterator_type>(), std::declval<typename has_begin<T>::iterator_value_type>()))
	>>: std::true_type {};

}


template <typename Buffer, typename SFINAE = void>
struct buffer_traits;

template <typename Buffer>
struct buffer_traits<Buffer, std::enable_if_t<
	detail::has_push_back<Buffer>::value
>>
{
	using char_type = std::decay_t<decltype(*std::begin(std::declval<Buffer>()))>;
	struct appender_type
	{
		template <typename Iterator, typename Sentinel>
		static bool append(Iterator begin, Sentinel end, Buffer &b, unsigned char min_width, char_type fill, char_type prefix)
		{
			if (prefix && fill != ' ')
				b.push_back(prefix);

			auto written_digits = std::distance(begin, end);
			while (min_width-- > written_digits)
				b.push_back(fill);

			if (prefix && fill == ' ')
				b.push_back(prefix);

			std::copy(begin, end, std::back_insert_iterator(b));

			return true;
		}
	};
};

template <typename Buffer>
struct buffer_traits<Buffer, std::enable_if_t<
	detail::has_string_append<Buffer>::value
>>
{
	using char_type = std::decay_t<decltype(*std::begin(std::declval<Buffer>()))>;
	struct appender_type
	{
		template <typename Iterator, typename Sentinel>
		static void append(Iterator begin, Sentinel end, Buffer &b, unsigned char min_width, char_type fill, char_type prefix)
		{
			if (prefix && fill != ' ')
				b.append(1, prefix);

			auto written_digits = std::distance(begin, end);
			if (min_width > written_digits)
				b.append(written_digits-min_width, fill);

			if (prefix && fill == ' ')
				b.append(1, prefix);

			std::copy(begin, end, std::inserter(b, std::end(b)));
		}
	};
};

template <>
struct buffer_traits<fz::buffer>
{
	using char_type = std::decay_t<decltype(*std::declval<fz::buffer>().get())>;
	struct appender_type
	{
		template <typename Iterator, typename Sentinel>
		static void append(Iterator begin, Sentinel end, fz::buffer &b, unsigned char min_width, char_type fill, char_type prefix)
		{
			if (prefix && fill != ' ')
				b.append(prefix);

			auto written_digits = std::distance(begin, end);
			while (min_width-- > written_digits)
				b.append(fill);

			if (prefix && fill == ' ')
				b.append(prefix);

			std::copy(begin, end, b.get(written_digits));
			b.add(written_digits);
		}
	};
};

template <typename Buffer, typename CharT = typename buffer_traits<Buffer>::char_type, typename Appender = typename buffer_traits<Buffer>::appender_type, typename T, std::enable_if_t<
	std::is_integral_v<T> &&
	std::is_constructible_v<CharT, decltype(' ')>
>* = nullptr>
integral_op_result convert(T value, Buffer &b, uintmax_t scale = 1, unsigned char base = 10, unsigned char min_width = 0, std::decay_t<CharT> fill = CharT(' '), std::decay_t<CharT> prefix = CharT('\0'))    {
	if constexpr(std::is_signed_v<T>) {
		if (value < 0) {
			// Doing two's complement negation by hand, to avoid integer overflow.
			return convert(~static_cast<std::make_unsigned_t<T>>(value) + 1, b, scale, base, min_width, fill, CharT('-'));
		}
	}

	if (scale < 1)
		return integral_op_result::bad_scale;

	if (base < 2 || base > 36)
		return integral_op_result::bad_base;

	std::array<CharT, sizeof(T)*CHAR_BIT> number;

	auto integral_it = std::end(number);

	value /= scale;

	do {
		auto digit = value % base;
		auto ascii_digit = (base <= 10 || digit < 10) ? CharT('0'+digit) : CharT('a'+(digit-10));

		*--integral_it = ascii_digit;
	} while (value /= base);

	Appender::append(integral_it, std::end(number), b, min_width, fill, prefix);

	return integral_op_result::ok;
}

}

#endif // FZ_UTIL_INTEGRAL_OPS_HPP
