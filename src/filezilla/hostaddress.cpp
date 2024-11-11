#include "hostaddress.hpp"
#include "util/bits.hpp"

namespace fz {

template <typename Host>
inline std::size_t calcprefix(const Host &from, const Host &to)
{
	std::size_t subnet_width = 0;
	std::size_t partial_subnet_width = 0;

	std::size_t i = Host::width/Host::xtet_width;

	do {
		i -= 1;

		auto partial_subnet_width_from = util::count_trailing_zeros(from[i]);
		auto partial_subnet_width_to = util::count_trailing_ones(to[i]);

		partial_subnet_width = std::min(partial_subnet_width_from, partial_subnet_width_to);

		subnet_width += partial_subnet_width;
	} while (partial_subnet_width == Host::xtet_width && i != 0);

	do {
		if (((from[i] ^ to[i]) >> partial_subnet_width) != 0)
			return std::string::npos;

		partial_subnet_width = 0;
	} while (i--);

	return Host::width-subnet_width;
}

template <typename Host>
std::string hostaddress::range<Host>::to_string() const
{
	std::string ret;

	auto prefix = calcprefix(from, to);

	ret = from.to_string();

	if (prefix == std::string::npos || (prefix == Host::width && to != from))
		ret.append("-").append(to.to_string());
	else
	if (prefix != Host::width)
		ret.append("/").append(std::to_string(prefix));

	return ret;
}

template<>
std::string hostaddress::ip<4>::to_string() const
{
	return fz::sprintf("%d.%d.%d.%d", (*this)[0], (*this)[1], (*this)[2], (*this)[3]);
}

template<>
std::string hostaddress::ip<6>::to_string() const
{
	static constexpr auto is_zero = [](auto v) constexpr { return v == 0; };

	difference_type max_span = 0;
	const_iterator max_zero_it = cend();

	for (const_iterator zero_it = begin(), non_zero_it; (zero_it = std::find_if(zero_it, cend(), is_zero)) != cend(); zero_it = non_zero_it) {
		non_zero_it = std::find_if_not(zero_it+1, cend(), is_zero);
		auto span = non_zero_it - zero_it;

		if (span > max_span) {
			max_span = span;
			max_zero_it = zero_it;
		}
	}

	std::string ret;

	for (auto it = cbegin(); it != cend(); ++it) {
		if (it < max_zero_it || max_zero_it+max_span <= it) {
			ret.append(fz::sprintf("%x", *it));

			if (it != cend()-1)
				ret.append(1, ':');
		}
		else
		if (it == cbegin() || it == max_zero_it+max_span-1)
			ret.append(1, ':');
	}

	return ret;
}

native_string hostaddress::host() const {
	if (auto *h = ipv4())
		return fz::to_native(h->to_string());

	if (auto *h = ipv6())
		return fz::to_native(h->to_string());

	if (unknown())
		return fzT("unknown");

	return {};
}

namespace {

struct address_comparison {
	enum class result {
		different_types,
		same,
		lhs_less_than_rhs,
		rhs_less_than_lhs,
		lhs_is_any,
		rhs_is_any
	};

	template <std::size_t V>
	result operator()(const hostaddress::ip<V> &lhs, const hostaddress::ip<V> &rhs)
	{
		static const hostaddress::ip<V> any{};

		if (lhs == rhs)
			return result::same;

		if (lhs == any)
			return result::lhs_is_any;

		if (rhs == any)
			return result::rhs_is_any;

		if (lhs < rhs)
			return result::lhs_less_than_rhs;

		return result::rhs_less_than_lhs;
	}

	template <typename T, typename U>
	result operator()(const T &, const U &)
	{
		if constexpr (std::is_same_v<T, U>)
			return result::same;
		else
			return result::different_types;
	}
};

}

bool hostaddress::equivalent_to(const hostaddress &rhs, bool any_is_equivalent) {
	if (port_ != rhs.port_)
		return false;

	auto res = std::visit(address_comparison(), host_, rhs.host_);

	switch (res) {
		case address_comparison::result::different_types:
		case address_comparison::result::lhs_less_than_rhs:
		case address_comparison::result::rhs_less_than_lhs:
			return false;

		case address_comparison::result::same:
			return true;

		case address_comparison::result::lhs_is_any:
		case address_comparison::result::rhs_is_any:
			return any_is_equivalent;
	}

	assert(false && "Invalid res");
	return false;
}

bool hostaddress::less_than(const hostaddress &rhs, bool any_is_equivalent)
{
	auto res = std::visit(address_comparison(), host_, rhs.host_);

	switch (res) {
		case address_comparison::result::different_types:
			return host_.index() < rhs.host_.index();

		case address_comparison::result::lhs_less_than_rhs:
			return true;

		case address_comparison::result::rhs_less_than_lhs:
			return false;

		case address_comparison::result::rhs_is_any:
			return any_is_equivalent && port_ < rhs.port_;

		case address_comparison::result::lhs_is_any:
			return !any_is_equivalent || port_ < rhs.port_;

		case address_comparison::result::same:
			return port_ < rhs.port_;
	}

	assert(false && "Invalid res");
	return false;
}

template struct hostaddress::ip<4>;
template struct hostaddress::ip<6>;
template class hostaddress::range<fz::hostaddress::ipv4_host>;
template class hostaddress::range<fz::hostaddress::ipv6_host>;

}
