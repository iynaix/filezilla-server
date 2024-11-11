#ifndef FZ_TCP_BINARY_BLACK_LIST_HPP
#define FZ_TCP_BINARY_BLACK_LIST_HPP

#include <string>
#include <vector>

#include <libfilezilla/rwmutex.hpp>

#include "../hostaddress.hpp"
#include "address_list.hpp"

namespace fz::tcp {

class binary_address_list: public address_list {
public:
	binary_address_list() = default;
	void set_thresholds(std::size_t ipv4_threshold, std::size_t ipv6_threshold);

	binary_address_list(binary_address_list &&rhs);
	binary_address_list(const binary_address_list &rhs);

	binary_address_list &operator=(binary_address_list &&rhs);
	binary_address_list &operator=(const binary_address_list &rhs);

	bool contains(std::string_view address, address_type family) const override;
	bool add(std::string_view address, address_type family) override;
	bool remove(std::string_view address, address_type family) override;
	std::size_t size() const override;

	/****************/

	using on_convert_error_type = std::function<bool (std::size_t idx, const std::string_view &)>;
	using on_convert_error_type_w = std::function<bool (std::size_t idx, const std::wstring_view &)>;

	template <typename ForwardIt, typename Sentinel, typename ErrorHandler>
	friend bool convert(const ForwardIt it, const Sentinel end, binary_address_list &res, const ErrorHandler &on_error);

	std::string to_string() const;

	class serializer;

private:
	using ipv4_range = fz::hostaddress::range<>::ipv4_range;
	using ipv6_range = fz::hostaddress::range<>::ipv6_range;

	std::vector<ipv4_range> ipv4_list_;
	std::vector<ipv6_range> ipv6_list_;

	std::size_t ipv4_threshold_ = 0;
	std::size_t ipv6_threshold_ = 0;

	mutable fz::rwmutex mutex_;
};


bool convert(const std::string_view &str, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error = {});
bool convert(const std::vector<std::string_view> &v, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error = {});
bool convert(const std::vector<std::string> &v, binary_address_list &res, const binary_address_list::on_convert_error_type &on_error = {});
bool convert(const std::vector<std::wstring_view> &v, binary_address_list &res, const binary_address_list::on_convert_error_type_w &on_error = {});

}

#endif // FZ_TCP_BINARY_BLACK_LIST_HPP
