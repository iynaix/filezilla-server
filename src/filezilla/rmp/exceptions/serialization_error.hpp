#ifndef FZ_RMP_EXCEPTION_SERIALIZATION_ERROR_HPP
#define FZ_RMP_EXCEPTION_SERIALIZATION_ERROR_HPP

#include <libfilezilla/format.hpp>

#include "generic.hpp"

namespace fz::rmp::exception {

struct serialization_error: generic
{
	template <typename ArchiveError, std::enable_if_t<std::is_convertible_v<ArchiveError, int>>* = nullptr>
	serialization_error(const ArchiveError & ae)
		: generic(get_description(ae))
		, error_(ae)
	{}

	serialization_error() = default;

	int error() const
	{
		return error_;
	}

	template <typename Archive>
	void serialize(Archive &ar)
	{
		using namespace serialization;

		if (ar(base_class<generic>(this))) {
			ar(nvp(error_, "error"));
		}
	}

protected:
	serialization_error(std::string description, int error)
		: generic(std::move(description))
		, error_(error)
	{}

private:
	template <typename ArchiveError>
	static auto get_description(const ArchiveError & ae, ...) -> std::string
	{
		std::string description;

		switch ((int)ae)
		{
			case EBADMSG: description = "Protocol versions don't match"; break;
			case ENODATA: description = "Not enough data"; break;
			case ENOENT:  description = "Element not found"; break;
			case ENOBUFS: description = "Buffer size exceeded"; break;
			case ERANGE:  description = "Value out of range"; break;
			case EINVAL:  description = "Value not allowed"; break;
			case ENOSYS:  description = "Message not handled"; break;
			default:      description = fz::sprintf("Serialization error %d", (int)ae);
		}

		return description;
	}

	template <typename ArchiveError>
	static auto get_description(const ArchiveError & ae) -> decltype(ae.description())
	{
		return ae.description();
	}

	int error_{};
};

}

#endif // FZ_RMP_EXCEPTION_SERIALIZATION_ERROR_HPP
