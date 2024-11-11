#ifndef FZ_SERIALIZATION_TYPES_BINARY_ADDRESS_LIST_HPP
#define FZ_SERIALIZATION_TYPES_BINARY_ADDRESS_LIST_HPP

#include "../../tcp/binary_address_list.hpp"

#include "containers.hpp"
#include "hostaddress.hpp"
#include "optional.hpp"

namespace fz::tcp {

class binary_address_list::serializer
{
public:
	serializer(tcp::binary_address_list &bl)
		: bl_(bl)
	{}

	template <typename Archive, serialization::trait::enable_if<!serialization::trait::is_textual_v<Archive>> = serialization::trait::sfinae>
	void serialize(Archive &ar)
	{
		ar(fz::serialization::optional_nvp(bl_.ipv4_list_, "ipv4", "range"));
		ar(fz::serialization::optional_nvp(bl_.ipv6_list_, "ipv6", "range"));

		if constexpr (fz::serialization::trait::is_input_v<Archive>) {
			std::sort(bl_.ipv4_list_.begin(), bl_.ipv4_list_.end());
			std::sort(bl_.ipv6_list_.begin(), bl_.ipv6_list_.end());
		}
	}

	template <typename Archive, serialization::trait::enable_if<serialization::trait::is_textual_v<Archive>> = serialization::trait::sfinae>
	auto save_minimal(const Archive &, typename Archive::error_t &) {
		return bl_.to_string();
	}

	template <typename Archive, serialization::trait::enable_if<serialization::trait::is_textual_v<Archive>> = serialization::trait::sfinae>
	void load_minimal(const Archive &ar, const std::string &s, typename Archive::error_t &error) {
		auto res = convert(s, bl_);
		if (!res)
			error = ar.error(EINVAL);
	}

private:
	tcp::binary_address_list &bl_;
};

template <typename Archive>
void serialize(Archive &ar, tcp::binary_address_list &bl)
{
	ar(fz::serialization::nvp(binary_address_list::serializer(bl), ""));
}

}

#endif // FZ_SERIALIZATION_TYPES_BINARY_ADDRESS_LIST_HPP
