#ifndef FZ_SERIALIZATION_TYPES_UPDATE_HPP
#define FZ_SERIALIZATION_TYPES_UPDATE_HPP

#include "../../../filezilla/update/info.hpp"
#include "../../../filezilla/update/checker.hpp"

#include "../helpers.hpp"

namespace fz::update {

template <typename Archive>
void save(Archive &ar, const info &i)
{
	using namespace fz::serialization;

	ar.attribute(i.get_flavour(), "flavour")(
		nvp(fz::to_string(i), "")
	);
}

template <typename Archive>
void load(Archive &ar, info &i)
{
	using namespace fz::serialization;

	std::string raw_data;
	fz::build_info::flavour_type flavour{};

	ar.attribute(flavour, "flavour")(
		nvp(raw_data, "")
	);

	if (ar)
		i = info(raw_data, flavour, allow::everything, get_null_logger());
}

template <typename Archive>
void serialize(Archive &ar, checker::options &opts)
{
	using namespace fz::serialization;

	ar(
		optional_nvp(opts.allowed_type(), "allowed_type"),
		optional_nvp(opts.frequency(), "frequency"),
		optional_nvp(opts.callback_path(), "callback_path")
	);
}

}

#endif // FZ_SERIALIZATION_TYPES_UPDATE_HPP
