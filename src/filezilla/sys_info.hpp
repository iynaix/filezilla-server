#ifndef FZ_SYS_INFO_HPP
#define FZ_SYS_INFO_HPP

#include <string>
#include <vector>

#include <libfilezilla/format.hpp>

namespace fz::sys_info
{

struct os_version_info
{
	unsigned int major{};
	unsigned int minor{};

	explicit operator bool() const
	{
		return major != 0;
	}

	template <typename String>
	friend String toString(const os_version_info &vi)
	{
		return fz::sprintf(fzS(typename String::value_type, "%s.%s"), vi.major, vi.minor);
	}

	operator std::string () const
	{
		return toString<std::string>(*this);
	}

	operator std::wstring () const
	{
		return toString<std::wstring>(*this);
	}
};

extern const os_version_info os_version;
extern const std::vector<std::string> cpu_caps;


}

#endif // FZ_SYS_INFO_HPP
