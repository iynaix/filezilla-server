#ifndef FZ_SERIALIZATION_HOSTADDRESS_HPP
#define FZ_SERIALIZATION_HOSTADDRESS_HPP

#include "../../hostaddress.hpp"
#include "../trait.hpp"
#include "../archives/xml.hpp"

namespace fz::serialization {

template <typename Archive, std::size_t Version, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
inline std::string save_minimal(const Archive&, const hostaddress::ip<Version> &host, typename Archive::error_t &)
{
	return host.to_string();
}

template <typename Archive, std::size_t Version, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
inline void load_minimal(const Archive &ar, hostaddress::ip<Version> &host, const std::string &ip, typename Archive::error_t &error)
{
	hostaddress h(ip, host.format);

	if (auto addr = h.get<hostaddress::ip<Version>>())
		host = *addr;
	else
		error = ar.error(EINVAL);
}

template <typename Archive, std::size_t Version, trait::enable_if<!trait::is_textual_v<Archive>> = trait::sfinae>
inline void serialize(Archive &ar, hostaddress::ip<Version> &host)
{
	ar(binary_data(host.data(), host.size()));
}

template <typename Archive, typename Host, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
inline std::string save_minimal(const Archive&, const hostaddress::range<Host> &range, typename Archive::error_t &)
{
	return range.to_string();
}

template <typename Archive, typename Host, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
inline void load_minimal(const Archive &ar, hostaddress::range<Host> &range, const std::string &str, typename Archive::error_t &error)
{
	hostaddress::range<Host> tmp(str);

	if (tmp)
		range = std::move(tmp);
	else
		error = ar.error(EINVAL);
}

template <typename Archive, typename Host, trait::enable_if<!trait::is_textual_v<Archive>> = trait::sfinae>
void serialize(Archive &ar, fz::hostaddress::range<Host> &range) {
	ar(nvp(range.from, "from"), nvp(range.to, "to"));

	if (!range)
		ar.error(EINVAL);
}

}

namespace fz::serialization {

template <typename Archive, std::size_t Version>
struct specialize<
	Archive,
	fz::hostaddress::ip<Version>,
	specialization::unversioned_nonmember_load_save_minimal_with_error,
	std::enable_if_t<trait::is_textual_v<Archive>>
>{};

template <typename Archive, std::size_t Version>
struct specialize<
	Archive,
	fz::hostaddress::ip<Version>,
	specialization::unversioned_nonmember_serialize,
	std::enable_if_t<!trait::is_textual_v<Archive>>
>{};


}

#endif // FZ_SERIALIZATION_HOSTADDRESS_HPP
