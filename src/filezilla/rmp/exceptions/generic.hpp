#ifndef FZ_RMP_EXCEPTIONS_GENERIC_HPP
#define FZ_RMP_EXCEPTIONS_GENERIC_HPP

#include <string>
#include "../../serialization/helpers.hpp"

namespace fz::rmp::exception {

struct generic
{
	generic(std::string description = {})
		: description_(std::move(description))
	{}

	const std::string description() const
	{
		return description_;
	}

	template <typename Archive>
	void serialize(Archive &ar)
	{
		using namespace serialization;

		ar(nvp(description_, "description"));
	}

private:
	std::string description_;
};

}
#endif // FZ_RMP_EXCEPTIONS_GENERIC_HPP
