#ifndef FZ_UTIL_PARSER_HPP
#define FZ_UTIL_PARSER_HPP

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <climits>
#include <iterator>
#include <limits>

#include "traits.hpp"

namespace fz::util {

template <
	typename Iterator,
	typename End = Iterator,
	typename Contiguous = std::false_type,
	typename CharT = std::decay_t<decltype(*std::declval<Iterator>())>
>
struct parseable_range
{
	using iterator = Iterator;
	using char_type = CharT;

	static_assert(CHAR_BIT == 8, "We won't work with anything but a 8 bits char, sorry!");
	static_assert(std::is_convertible_v<decltype('0'), CharT>);
	static_assert(std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>);

	template <typename Str, std::enable_if_t<
		std::is_constructible_v<Iterator, decltype(std::cbegin(std::declval<Str>()))>
	>* = nullptr>
	constexpr parseable_range(const Str &str) noexcept
		: it(std::cbegin(str))
		, end(std::cend(str))
	{
	}

	constexpr parseable_range(Iterator begin, End end) noexcept
		: it(begin)
		, end(end)
	{}

	constexpr parseable_range(const CharT *str)
		: it(str)
		, end(it+std::char_traits<CharT>::length(str))
	{}

	Iterator it;
	End end;

	inline static constexpr bool is_contiguous = Contiguous::value;
};

template <typename Str, typename Iterator = decltype(std::cbegin(std::declval<Str>())), typename End = decltype(std::cend(std::declval<Str>()))>
parseable_range(Str && str) -> parseable_range<Iterator, End, std::bool_constant<is_contiguous_v<std::decay_t<Str>> || std::is_array_v<Str>>>;

template <typename CharT, typename Iterator = const CharT *, typename End = const CharT *>
parseable_range(const CharT *str) -> parseable_range<Iterator, End, std::true_type>;

template <typename Iterator, typename End, std::void_t<decltype(std::declval<Iterator>() != std::declval<End>())>* = nullptr>
parseable_range(Iterator begin, End end) -> parseable_range<Iterator, End, std::conjunction<std::is_same<Iterator, End>, std::is_pointer<Iterator>>>;

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(parseable_range)

#define FZ_UTIL_PARSER(...)                                                                                                         \
	template <typename Range __VA_ARGS__,                                                                                           \
			  std::enable_if_t<::fz::util::trait::is_a_parseable_range_v<std::decay_t<Range>>>* = nullptr,                          \
			  typename Iterator = typename std::decay_t<Range>::iterator, typename CharT = typename std::decay_t<Range>::char_type, \
			  typename View = std::basic_string_view<CharT>>                                                                        \
	constexpr bool                                                                                                                  \
/***/

FZ_UTIL_PARSER( ) eol(Range &r)
{
	return r.it == r.end;
}

FZ_UTIL_PARSER( ) lit(Range &r, typename Range::char_type ch)
{
	return (!eol(r) && *r.it == ch) ? (++r.it, true) : false;
}

FZ_UTIL_PARSER( ) lit(Range &r, std::basic_string_view<CharT> what)
{
	if (!eol(r) && what.find(*r.it) != what.npos)  {
		++r.it;
		return true;
	}

	return false;
}

FZ_UTIL_PARSER( ) seq(Range &r, std::basic_string_view<CharT> what)
{
	auto begin = r.it;

	for (auto &w: what) {
		if (!lit(r, w)) {
			r.it = begin;
			return false;
		}
	}

	return true;
}

FZ_UTIL_PARSER( ) lit(Range &r, CharT from, CharT to)
{
	return (!eol(r) && from <= *r.it && *r.it <= to) ? (++r.it, true) : false;
}

FZ_UTIL_PARSER( ) until_lit(Range &r, std::initializer_list<CharT> chs, bool accept_eol = false)
{
	auto begin = r.it;
	bool eol_found = false;

	for (; !(eol_found = eol(r)); ++r.it) {
		if (std::find(chs.begin(), chs.end(), *r.it) != chs.end()) {
			return true;
		}
	}

	if (accept_eol && eol_found)
		return true;

	r.it = begin;
	return false;
}

FZ_UTIL_PARSER(, typename Begin, typename End) until_not_lit(Range &r, Begin b, End e)
{
	auto begin = r.it;

	for (; !eol(r); ++r.it) {
		if (std::find(b, e, *r.it) == e) {
			return true;
		}
	}

	r.it = begin;
	return false;
}

FZ_UTIL_PARSER( ) until_not_lit(Range &r, std::initializer_list<CharT> chs)
{
	return until_not_lit(r, chs.begin(), chs.end());
}

FZ_UTIL_PARSER( ) until_not_lit(Range &r, std::basic_string_view<CharT> chs)
{
	return until_not_lit(r, chs.begin(), chs.end());
}

FZ_UTIL_PARSER( ) until_lit(Range &r, CharT ch, bool accept_eol = false)
{
	return until_lit(r, {ch}, accept_eol);
}

FZ_UTIL_PARSER( ) parse_lit(Range &r, typename Range::char_type &ch, typename Range::char_type from=typename Range::char_type('\0'), typename Range::char_type to=std::numeric_limits<typename Range::char_type>::max())
{
	if (!eol(r)) {
		if (auto v = *r.it; from <= v && v <= to) {
			ch = v; ++r.it;
			return true;
		}
	}

	return false;
}

FZ_UTIL_PARSER( ) parse_lit(Range &r, typename Range::char_type &ch, std::basic_string_view<typename Range::char_type> accepted)
{
	if (!eol(r) && accepted.find(*r.it) != accepted.npos)  {
		ch = *(r.it++);
		return true;
	}

	return false;
}

FZ_UTIL_PARSER(, std::enable_if_t<Range::is_contiguous>* = nullptr) parse_until_lit(Range &r, std::basic_string_view<typename Range::char_type> &view, std::initializer_list<typename Range::char_type> chs, bool accept_eol = false)
{
	if (auto begin = r.it; until_lit(r, chs, accept_eol)) {
		view = { std::addressof(*begin), std::size_t(r.it-begin) };
		return true;
	}

	return false;
}

FZ_UTIL_PARSER(, std::enable_if_t<Range::is_contiguous>* = nullptr) parse_until_not_lit(Range &r, std::basic_string_view<typename Range::char_type> &view, std::initializer_list<typename Range::char_type> chs)
{
	if (auto begin = r.it; until_not_lit(r, chs)) {
		view = { std::addressof(*begin), std::size_t(r.it-begin) };
		return true;
	}

	return false;
}

FZ_UTIL_PARSER(, std::enable_if_t<Range::is_contiguous>* = nullptr) parse_until_not_lit(Range &r, std::basic_string_view<typename Range::char_type> &view, std::basic_string_view<typename Range::char_type> chs)
{
	if (auto begin = r.it; until_not_lit(r, chs)) {
		view = { std::addressof(*begin), std::size_t(r.it-begin) };
		return true;
	}

	return false;
}

FZ_UTIL_PARSER(, std::enable_if_t<Range::is_contiguous>* = nullptr) parse_until_eol(Range &r, std::basic_string_view<CharT> &view)
{
	view = { std::addressof(*r.it), std::size_t(r.end-r.it) };
	return true;
}

FZ_UTIL_PARSER( ) match_string(Range &r, std::basic_string_view<typename Range::char_type> str)
{
	auto old_it = r.it;

	for (CharT c: str) {
		if (!lit(r, c)) {
			r.it = old_it;
			return false;
		}
	}

	return true;
}

FZ_UTIL_PARSER( ) parse_until_string(Range &r, std::basic_string_view<typename Range::char_type> &view, std::basic_string_view<typename Range::char_type> str, bool accept_eol = false)
{
	auto begin = r.it;

	for (auto ch: str) {
		if (!until_lit(r, {ch}, accept_eol)) {
			r.it = begin;
			return false;
		}
	}

	view = { std::addressof(*begin), std::size_t(r.it-begin) };
	return true;
}

FZ_UTIL_PARSER(, typename ValueT) parse_int(Range &&range, ValueT &value, std::uint8_t base = 10, bool *overflow = nullptr)
{
	static_assert(std::is_integral_v<ValueT>, "value must be an integral");
	using UValueT = std::make_unsigned_t<ValueT>;

	auto char_to_num = [](CharT ch) -> UValueT {
		if (CharT('0') <= ch && ch <= CharT('9'))
			return UValueT(ch-CharT('0'));

		if (CharT('a') <= ch && ch <= CharT('z'))
			return UValueT(10)+UValueT(ch-CharT('a'));

		if (CharT('A') <= ch && ch <= CharT('Z'))
			return UValueT(10)+UValueT(ch-CharT('A'));

		// This ought not to be reached.
		return std::numeric_limits<UValueT>::max();
	};

	auto raise_and_add = [base, overflow](UValueT &value, auto addend, bool is_negative) {
		auto max = UValueT(std::numeric_limits<ValueT>::max() + is_negative);

		if (base > 0 && value > max / base) {
			if (overflow)
				*overflow = true;

			return false; // Overflow
		}

		UValueT x = value*base;

		if (max - x < addend) {
			if (overflow)
				*overflow = true;

			return false; // Overflow
		}

		value = UValueT(x+addend);

		return true;
	};

	auto matches = [base](CharT ch) {
		if (1 <= base && base <= 10)
			return CharT('0') <= ch && ch <= (CharT('0' + base-1));

		if (11 <= base && base <= 36)
			return
				(CharT('0') <= ch && ch <= (CharT('0' + 9))) ||
				(CharT('a') <= ch && ch <= (CharT('a' + base-11))) ||
				(CharT('A') <= ch && ch <= (CharT('A' + base-11)));

		return false;
	};

	bool result = false;
	bool is_negative = std::is_signed_v<ValueT> && lit(range, CharT('-'));

	if (!eol(range)) {
		if (auto ch = *range.it; matches(ch)) {
			auto old_it = range.it;
			result = true;
			UValueT v = 0;

			do {
				if (!raise_and_add(v, char_to_num(ch), is_negative)) {
					range.it = old_it;
					return false;
				}

				++range.it;
			} while (!eol(range) && (ch = *range.it, matches(ch)));

			value = ValueT(v*(!is_negative - is_negative));
		}
	}

	return result;
}

FZ_UTIL_PARSER(, typename ValueT) parse_int(Range &range, std::optional<ValueT> &value, std::uint8_t base = 10, bool *overflow = nullptr)
{
	if (ValueT v{}; parse_int(range, v, base, overflow)) {
		value = v;
		return true;
	}

	return false;
}

FZ_UTIL_PARSER(, typename ValueT) parse_int(Range &&range, std::size_t num_digits, ValueT &value, std::uint8_t base = 10, bool *overflow = nullptr)
{
	auto begin = range.it;

	if (parse_int(std::forward<Range>(range), value, base, overflow))
		return std::size_t(std::distance(begin, range.it)) == num_digits;

	return false;
}

}

#endif // PARSER_HPP
