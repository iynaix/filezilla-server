#ifndef FZ_TCP_ADDRESS_INFO_HPP
#define FZ_TCP_ADDRESS_INFO_HPP

#include <string>
#include <tuple>

#include "../serialization/helpers.hpp"
#include "../hostaddress.hpp"

namespace fz::tcp {

struct address_info
{
	std::string address{};
	unsigned int port{};

	template <typename Archive>
	void serialize(Archive &ar)
	{
		ar(FZ_NVP(address), FZ_NVP(port));
	}

	bool less_than(const address_info &rhs, bool any_is_equivalent = hostaddress::any_is_equivalent) const
	{
		return hostaddress(address, hostaddress::format::ipvx, port).less_than(hostaddress(rhs.address, hostaddress::format::ipvx, rhs.port), any_is_equivalent);
	}

	auto static less_than(bool any_is_equivalent) {
		return [any_is_equivalent](const address_info &lhs, const address_info &rhs)
		{
			return lhs.less_than(rhs, any_is_equivalent);
		};
	}

	bool operator <(const address_info &rhs) const
	{
		return less_than(rhs);
	}

	bool operator ==(const address_info &rhs) const
	{
		return std::tie(address, port) == std::tie(rhs.address, rhs.port);
	}
};

}

#endif // FZ_TCP_ADDRESS_INFO_HPP
