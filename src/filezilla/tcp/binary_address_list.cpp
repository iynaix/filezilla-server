/* Various strategies can be thought of to implement a IP Black List, the tradeoff being between speed and memory
 * consumption. Literature suggests bloom filters might be an approach for large data sets, but it also suggests
 * they may have a big speed penalty due to lots of cache misses. Someone suggests using a bitmap for ipv4 addresses
 * (16 MiB) and some other suggest taking advantage of the address structure in order to implement a heuristic that
 * will not hurt too many innocent ipv6 addresses.
 *
 * A few links:
 *    https://serverfault.com/questions/919320/how-to-protect-a-web-application-from-ipv6-bots
 *    https://blog.cloudflare.com/when-bloom-filters-dont-bloom/
 *
 * The current implementation uses a binary search within a vector of coalesced and configurable ranges.
 * However, given the complexity of it and the above considerations, this approach should probably be rethought.
 */

/* Tests have been made with godbolt. See: https://godbolt.org/z/s88nz6 */

#ifndef FZ_TCP_BINARY_ADDRESS_LIST_IPP
#define FZ_TCP_BINARY_ADDRESS_LIST_IPP

#include "../util/traits.hpp"

#include "binary_address_list.hpp"

namespace fz::tcp {

namespace detail {

// left must be strictly < than right, and they both ought to have the same size.
inline auto distance_over_threshold = [](const auto &left, const auto &right, std::size_t threshold) constexpr -> std::size_t {
	using digit_t = std::decay_t<decltype(left[0])>;

	static_assert(sizeof(std::size_t) >= 2*sizeof(left[0]));
	static_assert(std::is_unsigned_v<digit_t>);

	constexpr std::size_t shift = 8*sizeof(left[0]);

	std::size_t i = 0;
	std::size_t diff = 0;
	std::size_t carry = 0;

	for (auto it_l = left.rbegin(), it_r = right.rbegin(); it_l != left.rend(); ++it_l, ++it_r, ++i) {
		std::size_t digit_l = *it_l;
		std::size_t digit_r = *it_r - carry;

		carry = digit_r < digit_l;
		digit_r += carry << shift;
		diff += (digit_r - digit_l) << (i*shift);

		if (diff > threshold)
			return diff;
	}

	return diff;
};

inline auto increment = [](auto &host) constexpr {
	using digit_t = std::decay_t<decltype(host[0])>;
	static_assert(std::is_unsigned_v<digit_t>);

	digit_t carry = 1;

	for (auto it = host.rbegin(); it != host.rend() && carry; ++it) {
		auto &d = *it;
		d += carry;
		carry = (d == 0);
	}
};

inline auto decrement = [](auto &host) constexpr {
	using digit_t = std::decay_t<decltype(host[0])>;
	static_assert(std::is_unsigned_v<digit_t>);

	digit_t carry = 1;

	for (auto it = host.rbegin(); it != host.rend() && carry; ++it) {
		auto &d = *it;
		d -= carry;
		carry = (d == digit_t(-1));
	}
};

inline auto insert_or_merge = [](auto &list, auto *host, std::size_t threshold) -> bool {
	if (host) {
		// Use binary search to find the range that is >= than the host we're dealing with.
		auto right = std::lower_bound(list.begin(), list.end(), *host);

		std::size_t distance_l = threshold;
		std::size_t distance_r = threshold;

		if (right != list.end()) {
			// If the host falls within the range we've found, there's nothing to do.
			if (!(*host < right->from))
				return false;

			distance_r = distance_over_threshold(*host, right->from, threshold);
		}

		if (right != list.begin()) {
			auto left = std::prev(right);
			distance_l = distance_over_threshold(left->to, *host, threshold);
		}

		if (distance_l < threshold && distance_l <= distance_r) {
			auto left = std::prev(right);
			left->to = *host;

			// If left and right differ only by one unit, it makes sense
			// to merge them into one. However, this takes more time.
			constexpr bool check_whether_we_need_to_merge_left_with_right = true;

			if (check_whether_we_need_to_merge_left_with_right && right != list.end()) {
				auto tmp = *host;
				increment(tmp);

				if (tmp == right->from) {
					left->to = right->to;
					list.erase(right);
				}
			}

			return true;
		}

		if (distance_r < threshold && distance_r <= distance_l) {
			right->from = *host;
			return true;
		}

		// Else insert the new range before the right one.
		list.emplace(right, *host, *host);
	}

	return true;
};

inline auto find_in_list = [] (const auto &list, const auto *host) {
	// Use binary search to find the range that is >= than the host we're dealing with.
	auto it = std::lower_bound(list.begin(), list.end(), *host);

	return (it != list.end()) && (!(*host < it->from));
};

inline auto remove_and_maybe_split = [] (auto &list, const auto *host) -> bool {
	// Use binary search to find the range that is >= than the host we're dealing with.
	auto it = std::lower_bound(list.begin(), list.end(), *host);

	if (it == list.end()) {
		// Not found, nothing to do.
		return false;
	}

	bool is_equivalent = (it != list.end()) && (!(*host < it->from));
	if (!is_equivalent) {
		// Again, not found, nothing to do.
		return false;
	}

	// So, we've found a range that contains our host.
	// We need to split the range in a way that excludes the given host,
	// EVEN if that would mean obtaining two ranges close to one another
	// within the threshold.
	if (it->from == it->to) {
		list.erase(it);
		return true;
	}

	auto left = it->from;
	it->from = *host;
	increment(it->from);

	it = list.insert(it, {left, *host});
	decrement(it->to);

	return true;
};

template <address_type Family>
struct family2host;

template <>
struct family2host<address_type::ipv4> {
	using type = hostaddress::ipv4_host;
};

template <>
struct family2host<address_type::ipv6> {
	using type = hostaddress::ipv6_host;
};

inline constexpr auto host = std::placeholders::_1;

template <address_type Family, typename Func, typename... Args>
inline bool invoke_with_host(std::string_view address, Func && f, Args &&... args)
{
	using host_t = typename family2host<Family>::type;

	auto host = hostaddress(address, host_t::format);
	auto ip = host.get<host_t>();

	if (!ip)
		return false;

	auto maybe_host = [ip](auto && arg) -> decltype(auto) {
		if constexpr (constexpr auto v = std::is_placeholder_v<std::decay_t<decltype(arg)>>; v > 0) {
			static_assert(v == 1, "Only placeholder _1 must be used");

			return ip;
		}
		else
			return std::forward<decltype(arg)>(arg);
	};

	return std::forward<Func>(f)(maybe_host(std::forward<Args>(args))...);
}

}

binary_address_list &binary_address_list::operator =(binary_address_list &&rhs)
{
	if (&rhs != this) {
		scoped_write_lock rhs_lock(rhs.mutex_);
		scoped_write_lock self_lock(mutex_);

		ipv4_list_ = std::move(rhs.ipv4_list_);
		ipv4_threshold_ = std::move(rhs.ipv4_threshold_);

		ipv6_list_ = std::move(rhs.ipv6_list_);
		ipv6_threshold_ = std::move(rhs.ipv6_threshold_);
	}

	return *this;
}

binary_address_list &binary_address_list::operator =(const binary_address_list &rhs)
{
	if (&rhs != this) {
		scoped_read_lock rhs_lock(rhs.mutex_);
		scoped_write_lock self_lock(mutex_);

		ipv4_list_ = rhs.ipv4_list_;
		ipv4_threshold_ = rhs.ipv4_threshold_;

		ipv6_list_ = rhs.ipv6_list_;
		ipv6_threshold_ = rhs.ipv6_threshold_;
	}

	return *this;
}

binary_address_list::binary_address_list(binary_address_list &&rhs)
{
	scoped_write_lock rhs_lock(rhs.mutex_);
	scoped_write_lock self_lock(mutex_);

	ipv4_list_ = std::move(rhs.ipv4_list_);
	ipv4_threshold_ = std::move(rhs.ipv4_threshold_);

	ipv6_list_ = std::move(rhs.ipv6_list_);
	ipv6_threshold_ = std::move(rhs.ipv6_threshold_);
}

binary_address_list::binary_address_list(const binary_address_list &rhs)
{
	scoped_read_lock rhs_lock(rhs.mutex_);
	scoped_write_lock self_lock(mutex_);

	ipv4_list_ = rhs.ipv4_list_;
	ipv4_threshold_ = rhs.ipv4_threshold_;

	ipv6_list_ = rhs.ipv6_list_;
	ipv6_threshold_ = rhs.ipv6_threshold_;
}


void binary_address_list::set_thresholds(std::size_t ipv4_threshold, std::size_t ipv6_threshold)
{
	scoped_write_lock lock{mutex_};

	ipv4_threshold_ = ipv4_threshold;
	ipv6_threshold_ = ipv6_threshold;
}

bool binary_address_list::contains(std::string_view address, address_type family) const
{
	scoped_read_lock lock{mutex_};

	bool try_ipv4 = true;
	bool try_ipv6 = true;
	bool success = false;

	if (family == address_type::ipv4)
		try_ipv6 = false;
	else
	if (family == address_type::ipv6)
		try_ipv4 = false;

	if (try_ipv4)
		success = !ipv4_list_.empty() && detail::invoke_with_host<address_type::ipv4>(address, detail::find_in_list, ipv4_list_, detail::host);

	if (!success && try_ipv6)
		success = !ipv6_list_.empty() && detail::invoke_with_host<address_type::ipv6>(address, detail::find_in_list, ipv6_list_, detail::host);

	return success;
}

bool binary_address_list::add(std::string_view address, address_type family)
{
	scoped_read_lock lock{mutex_};

	bool try_ipv4 = true;
	bool try_ipv6 = true;
	bool success = false;

	if (family == address_type::ipv4)
		try_ipv6 = false;
	else
	if (family == address_type::ipv6)
		try_ipv4 = false;

	if (try_ipv4)
		success = detail::invoke_with_host<address_type::ipv4>(address, detail::insert_or_merge, ipv4_list_, detail::host, ipv4_threshold_);

	if (!success && try_ipv6)
		success = detail::invoke_with_host<address_type::ipv6>(address, detail::insert_or_merge, ipv6_list_, detail::host, ipv6_threshold_);

	return success;
}

bool binary_address_list::remove(std::string_view address, address_type family)
{
	scoped_read_lock lock{mutex_};

	bool try_ipv4 = true;
	bool try_ipv6 = true;
	bool success = false;

	if (family == address_type::ipv4)
		try_ipv6 = false;
	else
	if (family == address_type::ipv6)
		try_ipv4 = false;

	if (try_ipv4)
		success =!ipv4_list_.empty() && detail::invoke_with_host<address_type::ipv4>(address, detail::remove_and_maybe_split, ipv4_list_, detail::host);

	if (!success && try_ipv6)
		success = !ipv6_list_.empty() && detail::invoke_with_host<address_type::ipv6>(address, detail::remove_and_maybe_split, ipv6_list_, detail::host);

	return success;
}

std::size_t binary_address_list::size() const
{
	scoped_read_lock lock{mutex_};

	return ipv4_list_.size() + ipv6_list_.size();
}

template <typename ForwardIt, typename Sentinel, typename ErrorHandler>
bool convert(ForwardIt it, const Sentinel end, binary_address_list &res, const ErrorHandler &on_error)
{
	res.ipv4_list_.clear();
	res.ipv6_list_.clear();

	for (std::size_t i = 0; it != end; ++i, ++it) {
		if (util::parseable_range r(*it); lit(r, '*') && eol(r)) {
			res.ipv4_list_.push_back(hostaddress::range<hostaddress::ipv4_host>::full());
			res.ipv6_list_.push_back(hostaddress::range<hostaddress::ipv6_host>::full());
		}
		else
		if (auto range = hostaddress::range<hostaddress::ipv4_host>(*it)) {
			res.ipv4_list_.push_back(std::move(range));
		}
		else
		if (auto range = hostaddress::range<hostaddress::ipv6_host>(*it)) {
			res.ipv6_list_.push_back(std::move(range));
		}
		else
		if (on_error && !on_error(i, *it))
			return false;
	}

	std::sort(res.ipv4_list_.begin(), res.ipv4_list_.end());
	std::sort(res.ipv6_list_.begin(), res.ipv6_list_.end());

	return true;
}


bool convert(const std::string_view &str, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error)
{
	return convert(fz::strtok_view(str, " \t\r\n;,"), res, on_error);
}

bool convert(const std::vector<std::string_view> &v, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error)
{
	return convert(v.begin(), v.end(), res, on_error);
}

bool convert(const std::vector<std::string> &v, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error)
{
	return convert(v.begin(), v.end(), res, on_error);
}

bool convert(const std::vector<std::wstring_view> &v, binary_address_list &res, const binary_address_list::on_convert_error_type_w &on_error)
{
	return convert(v.begin(), v.end(), res, on_error);
}

std::string binary_address_list::to_string() const
{
	std::string str;

	for (const auto &r: ipv4_list_) {
		str.append(r.to_string()).append(1, ' ');
	}

	for (const auto &r: ipv6_list_) {
		str.append(r.to_string()).append(1, ' ');
	}

	if (!str.empty())
		str.resize(str.size()-1);

	return str;
}

}

#endif
