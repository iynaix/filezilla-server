#ifndef FZ_HOSTADDRESS_HPP
#define FZ_HOSTADDRESS_HPP

#include <array>
#include <variant>
#include <optional>
#include <climits>
#include <tuple>
#include <cassert>

#include <libfilezilla/iputils.hpp>
#include <libfilezilla/format.hpp>

#include "util/parser.hpp"

namespace fz {

class hostaddress
{
public:
	enum class format { port_cmd = 1, eprt_cmd = 2, ipv4 = 3, ipv6 = 4, ipvx = 5};

	template <std::size_t Version, typename AP = void>
	struct ip;

	template <typename T, std::size_t N>
	struct ip_base: public std::array<T, N> {
		static constexpr std::uint8_t xtet_width = sizeof(T) * CHAR_BIT;
		static constexpr std::uint8_t width = N * xtet_width;
	};

	template <typename AP>
	struct ip<4, AP>: ip_base<std::uint8_t, 4> {
		static constexpr auto format = hostaddress::format::ipv4;

		std::string to_string() const;
		std::uint32_t to_uint32() const
		{
			std::uint32_t ret = 0;

			for (auto i = 0u; i < 4u; ++i) {
				ret <<= 8;
				ret |= data()[i];
			}

			return ret;
		}
	};

	template <typename AP>
	struct ip<6, AP>: ip_base<std::uint16_t, 8> {
		static constexpr auto format = hostaddress::format::ipv6;

		std::string to_string() const;
		std::uint64_t high_to_uint64() const {
			std::uint64_t ret = 0;

			for (auto i = 0u; i < 4u; ++i) {
				ret <<= 16;
				ret |= data()[i];
			}

			return ret;
		}

		std::uint64_t low_to_uint64() const {
			std::uint64_t ret = 0;

			for (auto i = 4u; i < 8u; ++i) {
				ret <<= 16;
				ret |= data()[i];
			}

			return ret;
		}
	};

	using ipv4_host = ip<4>;
	using ipv6_host = ip<6>;

	struct unknown_host {};

	using host_t = std::variant<std::monostate, ipv4_host, ipv6_host, unknown_host>;

	template <typename Host = void>
	class range;

private:
	host_t host_{};
	std::uint16_t port_{};

public:
	constexpr hostaddress() noexcept {}

	constexpr hostaddress(host_t &&h, std::uint16_t p) noexcept
		:host_{std::move(h)}, port_{p}
	{}

	template <
		typename Str,
		typename Iterator = decltype(std::cbegin(std::declval<Str>())),
		typename CharT = std::decay_t<decltype(*std::declval<Iterator>())>,
		typename = std::enable_if_t<
			std::is_convertible_v<decltype('0'), CharT> &&
			std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
		>
	>
	constexpr hostaddress(const Str &str, format format, std::optional<std::uint16_t> port_override = std::nullopt) noexcept
		: hostaddress{parse(str, format, port_override)}
	{}

	constexpr const ipv4_host *ipv4() const {
		return std::get_if<ipv4_host>(&host_);
	}

	constexpr const ipv6_host *ipv6() const {
		return std::get_if<ipv6_host>(&host_);
	}

	constexpr const unknown_host *unknown() const {
		return std::get_if<unknown_host>(&host_);
	}

	template <typename Host>
	constexpr auto get() const -> decltype (std::get_if<Host>(&host_)){
		return std::get_if<Host>(&host_);
	}

	native_string host() const;

	constexpr std::uint16_t port() const {
		return port_;
	}

	constexpr bool is_valid() const {
		return !std::holds_alternative<std::monostate>(host_);
	}

	constexpr explicit operator bool() const {
		return is_valid();
	}

#ifdef FZ_WINDOWS
	static constexpr bool any_is_equivalent = true;
#else
	static constexpr bool any_is_equivalent = true;
#endif

	bool equivalent_to(const hostaddress &rhs, bool any_is_equivalent = hostaddress::any_is_equivalent);
	bool less_than(const hostaddress &rhs, bool any_is_equivalent = hostaddress::any_is_equivalent);

protected:
	template <typename Str>
	constexpr static hostaddress parse(Str &str, format format, std::optional<std::uint16_t> port_override);
};

template <typename Host>
class hostaddress::range
{
public:
	Host from;
	Host to;

	bool operator <(const Host &rhs) const {
		return from < rhs && to < rhs;
	}

	bool operator <(const range &rhs) const {
		return from < rhs.from && to < rhs.to;
	}

	constexpr range() noexcept = default;

	constexpr range(Host &&from, Host &&to) noexcept
		: from(std::move(from))
		, to(std::move(to))
	{}

	constexpr range(const Host &from, const Host &to) noexcept
		: from(from)
		, to(to)
	{}

	template <
		typename Str,
		typename Iterator = decltype(std::cbegin(std::declval<Str>())),
		typename CharT = std::decay_t<decltype(*std::declval<Iterator>())>,
		typename = std::enable_if_t<
			std::is_convertible_v<decltype('0'), CharT> &&
			std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
		>
	>
	constexpr range(const Str &str) noexcept
		:range(parse(str))
	{}

	constexpr bool is_valid() const {
		return from[0] <= to[0];
	}

	constexpr explicit operator bool() const {
		return is_valid();
	}

	std::string to_string() const;

	static range full()
	{
		range ret;

		std::fill(ret.from.begin(), ret.from.end(), typename Host::value_type(0));
		std::fill(ret.to.begin(), ret.to.end(), typename Host::value_type(-1));

		return ret;
	}

private:
	template <typename Str>
	constexpr static range parse(Str &str) noexcept;
};

template <>
class hostaddress::range<void>
{
public:
	using ipv4_range = range<ipv4_host>;
	using ipv6_range = range<ipv6_host>;

	using range_t = std::variant<std::monostate, ipv4_range, ipv6_range>;

	constexpr range() noexcept = default;

	constexpr range(range_t && r) noexcept
		: range_(std::move(r))
	{}

	template <
		typename Str,
		typename Iterator = decltype(std::cbegin(std::declval<Str>())),
		typename CharT = std::decay_t<decltype(*std::declval<Iterator>())>,
		typename = std::enable_if_t<
			std::is_convertible_v<decltype('0'), CharT> &&
			std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
		>
	>
	constexpr range(const Str &str) noexcept
		:range(parse(str))
	{}

	constexpr const ipv4_range *ipv4() const {
		return std::get_if<ipv4_range>(&range_);
	}

	constexpr const ipv6_range *ipv6() const {
		return std::get_if<ipv6_range>(&range_);
	}

	constexpr bool is_valid() const {
		return !std::holds_alternative<std::monostate>(range_);
	}

	constexpr explicit operator bool() const {
		return is_valid();
	}

private:
	template <typename Str>
	constexpr static range parse(Str &str) noexcept {
		if (auto range = ipv4_range(str))
			return { std::move(range) };

		if (auto range = ipv6_range(str))
			return { std::move(range) };

		return {};
	}

	range_t range_;
};

FZ_UTIL_PARSER( ) parse_eprt_type(Range &r, address_type &ipv)
{
	if (CharT ch{}; parse_lit(r, ch, CharT('0'), CharT('9'))) {
		if (ch == CharT('1')) {
			ipv = address_type::ipv4;
		}
		else
		if (ch == CharT('2')) {
			ipv = address_type::ipv6;
		}
		else {
			ipv = address_type::unknown;
		}

		return true;
	}

	return false;
}

FZ_UTIL_PARSER( ) parse_ip(Range &r, hostaddress::ipv4_host &ipv4h, CharT sep = '.')
{
	return
		parse_int(r, ipv4h[0]) && lit(r, sep) &&
		parse_int(r, ipv4h[1]) && lit(r, sep) &&
		parse_int(r, ipv4h[2]) && lit(r, sep) &&
		parse_int(r, ipv4h[3]);
}

FZ_UTIL_PARSER( ) parse_ip(Range &r, hostaddress::ipv6_host &ipv6h) {
	std::size_t double_colon_begin = ipv6h.size();

	bool has_bkt = lit(r, '[');

	std::size_t hextet_count = 0;
	while (hextet_count < ipv6h.size() && !eol(r)) {
		auto cur = r.it;
		if (parse_int(r, ipv6h[hextet_count], 16)) {
			++hextet_count;

			if (!lit(r, ':')) {
				if (lit(r, '.')) {
					if (hextet_count <= ipv6h.size()-1) {
						// Let's see if it's a ipv4 suffix
						--hextet_count;
						r.it = cur;

						if (hostaddress::ipv4_host ipv4h{}; parse_ip(r, ipv4h, '.')) {
							ipv6h[hextet_count++] = ipv4h[0]*256 + ipv4h[1];
							ipv6h[hextet_count++] = ipv4h[2]*256 + ipv4h[3];
							break;
						}
					}

					return false;
				}
				break;
			}
		}
		else
		if (double_colon_begin > hextet_count && lit(r, ':') && (hextet_count > 0 || lit(r, ':'))) {
			double_colon_begin = hextet_count;
		}
		else
			break;
	}

	if (double_colon_begin < ipv6h.size()) { // has double colon
		if (hextet_count > ipv6h.size()-1)
			return false; // too many hextets

		std::size_t double_colon_size = ipv6h.size()-hextet_count;
		std::size_t num_extets_to_move = hextet_count-double_colon_begin;

		auto back_cur = ipv6h.rbegin();
		auto rbegin = back_cur+std::ptrdiff_t(double_colon_size);
		auto rend = rbegin+std::ptrdiff_t(num_extets_to_move);

		for (auto it = rbegin; it != rend; ++it, ++back_cur) {
			*back_cur = *it;
		}

		while (double_colon_size--) {
			*back_cur++ = 0;
		}
	}
	else
	if (hextet_count < ipv6h.size())
		return false; // too few hextets

	return !has_bkt || lit(r, ']');
}

FZ_UTIL_PARSER( ) parse_eprt_cmd(Range &r, address_type &ipv, hostaddress::ipv4_host &ipv4h, hostaddress::ipv6_host &ipv6h, std::uint16_t &port)
{
	CharT sep{};

	return parse_lit(r, sep, CharT(33), CharT(126)) && parse_eprt_type(r, ipv) && lit(r, sep) && (
		ipv == address_type::ipv4 ? parse_ip(r, ipv4h, CharT('.')) :
		ipv == address_type::ipv6 ? parse_ip(r, ipv6h) :
		ipv == address_type::unknown ? until_lit(r, sep) :
		false
	) && lit(r, sep) && parse_int(r, port) && lit(r, sep);
}

FZ_UTIL_PARSER( ) parse_port_cmd(Range &r, hostaddress::ipv4_host &ipv4h, std::uint16_t &port)
{
	std::uint8_t phigh=0, plow=0;

	bool success =
		parse_ip(r, ipv4h, ',') &&
		lit(r, ',') && parse_int(r, phigh) &&
		lit(r, ',') && parse_int(r, plow);

	if (success)
		port = phigh*256 + plow;

	return success;
}

FZ_UTIL_PARSER(, typename Host) parse_ip_range(Range &r, Host &from, Host &to)
{
	if (lit(r, '*')) {
		// It's as if 0.0.0.0/0, i.e. the full range of addresses.

		std::fill(from.begin(), from.end(), typename Host::value_type(0));
		std::fill(to.begin(), to.end(), typename Host::value_type(-1));

		return true;
	}
	else
	if (parse_ip(r, from)) {
		// Range in the interval form: from-to, with from <= to.
		if (lit(r, '-')) {
			// We need this to keep keep constexpr happy
			auto less_than_or_equal = [](const Host &lhs, const Host &rhs) {
				for (std::size_t i = 0; i < std::tuple_size_v<typename Host::array>; ++i) {
					if (!(lhs[i] <= rhs[i]))
						return false;
				}

				return true;
			};

			if (parse_ip(r, to)) {
				if (!less_than_or_equal(from, to)) {
					Host tmp = std::move(from);
					from = std::move(to);
					to = std::move(tmp);
				}

				return true;
			}

			return false;
		}

		// Range in the CIDR form: ip/prefix
		if (lit(r, '/')) {
			std::uint8_t prefix_width = 0;

			if (parse_int(r, prefix_width) && prefix_width <= Host::width) {
				to = from;

				auto cur_to = std::rbegin(to);
				auto cur_from = std::rbegin(from);

				auto subnet_width = Host::width - prefix_width;
				auto to_fill = subnet_width/Host::xtet_width;

				while (to_fill-- > 0) {
					*cur_to++ = (1 << Host::xtet_width)-1;
					*cur_from++ = 0;
				}

				if (auto residual = subnet_width % Host::xtet_width) {
					auto residual_mask = (1 << residual)-1;

					*cur_to |= residual_mask;
					*cur_from &= ~residual_mask;
				}

				return true;
			}

			return false;
		}

		// A single address is also fine: it's as if it was written as ip/ip_size.
		to = from;
		return true;
	}

	return false;
}

template <typename Str>
constexpr hostaddress hostaddress::parse(Str &str, format format, std::optional<std::uint16_t> port_override) {
	address_type ipv{address_type::unknown};
	ipv4_host ipv4h{};
	ipv6_host ipv6h{};
	std::uint16_t port{};

	bool success = false;

	util::parseable_range r(str);

	if (format == format::eprt_cmd) {
		success = parse_eprt_cmd(r, ipv, ipv4h, ipv6h, port) && eol(r);
	}
	else
	if (format == format::port_cmd) {
		success = parse_port_cmd(r, ipv4h, port) && eol(r);
		if (success)
			ipv = address_type::ipv4;
	}
	else
	if (format == format::ipv4) {
		success = parse_ip(r, ipv4h, '.') && eol(r);
		if (success)
			ipv = address_type::ipv4;
	}
	else
	if (format == format::ipv6) {
		success = parse_ip(r, ipv6h) && eol(r);
		if (success)
			ipv = address_type::ipv6;
	}
	else
	if (format == format::ipvx) {
		auto old_r = r;
		success = parse_ip(r, ipv4h, '.') && eol(r);
		if (success)
			ipv = address_type::ipv4;
		else {
			r = std::move(old_r);

			success = parse_ip(r, ipv6h) && eol(r);

			if (success)
				ipv = address_type::ipv6;
		}
	}

	if (success) {
		if (port_override)
			port = *port_override;

		if (ipv == address_type::ipv4)
			return {std::move(ipv4h), port};

		if (ipv == address_type::ipv6)
			return {std::move(ipv6h), port};

		if (ipv == address_type::unknown)
			return {unknown_host{}, port};
	}

	return {};
}

template <typename Host>
template <typename Str>
constexpr hostaddress::range<Host> hostaddress::range<Host>::parse(Str &str) noexcept {
	util::parseable_range r(str);

	Host from{}, to{};
	if (!(parse_ip_range(r, from, to) && eol(r))) {
		from[0] = 1;
		to[0] = 0;
	}

	return {std::move(from), std::move(to)};
}

template<> std::string fz::hostaddress::ip<4>::to_string() const;
template<> std::string fz::hostaddress::ip<6>::to_string() const;

extern template struct fz::hostaddress::ip<4>;
extern template struct fz::hostaddress::ip<6>;
extern template class fz::hostaddress::range<fz::hostaddress::ipv4_host>;
extern template class fz::hostaddress::range<fz::hostaddress::ipv6_host>;

// Utility function that I didn't know where else to put.
template <typename String>
String join_host_and_port(const String &host, unsigned int port)
{
	using Char = typename String::value_type;

	util::parseable_range r(host);
	hostaddress::ipv6_host ipv6;

	auto format =
		host[0] != Char('[') && parse_ip(r, ipv6) && eol(r)
			? fzS(Char, "[%s]:%d")
			: fzS(Char, "%s:%d");

	return fz::sprintf(format, host, port);
}

}

#endif // HOSTADDRESS_HPP

