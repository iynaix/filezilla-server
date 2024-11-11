#ifndef FZ_SERIALIZATION_TYPES_VARIANT_HPP
#define FZ_SERIALIZATION_TYPES_VARIANT_HPP

#include <utility>
#include <cstdint>
#include <variant>

#include "../../covariant.hpp"
#include "../helpers.hpp"
#include "../../mpl/with_index.hpp"

namespace fz::serialization {

	namespace variant_detail
	{
		template<std::size_t I, typename Archive, typename Variant>
		bool load_value(Archive &ar, Variant &variant, std::uint16_t target_index) {
			if (I == target_index) {
				std::variant_alternative_t<I, Variant> value;

				if (ar(nvp(value, "")))
					variant = std::move(value);

				return true;
			}

			return false;
		}


		template<typename Archive, typename Variant, std::size_t ...Is>
		void load(Archive &ar, Variant &variant, std::uint16_t target_index, std::index_sequence<Is...>)
		{
			(load_value<Is>(ar, variant, target_index) || ...);
		}

		template<std::size_t I, typename Archive, typename Variant>
		bool load_value(Archive &ar, Variant &variant, const std::string &target_type) {
			using T = std::variant_alternative_t<I, Variant>;

			if (name_of_type<Archive, T>::value == target_type) {
				T value;

				if (ar(nvp(value, "")))
					variant = std::move(value);

				return true;
			}

			return false;
		}

		template<typename Archive, typename Variant, std::size_t ...Is>
		bool load(Archive &ar, Variant &variant, const std::string &target_type, std::index_sequence<Is...>)
		{
			return (load_value<Is>(ar, variant, target_type) || ...);
		}

	}

	template <typename Archive, typename T, typename... Ts> inline
	void save(Archive &ar, const std::variant<T, Ts...> &variant)
	{
		std::int16_t index = static_cast<std::int16_t>(variant.index());

		ar.optional_attribute(index, "index");

		std::visit([&ar](const auto &value){
			ar(nvp(value, ""));
		}, variant);
	}

	template <typename Archive, typename T, typename... Ts> inline
	void load(Archive &ar, std::variant<T, Ts...> &variant)
	{
		constexpr auto max_index = std::variant_size_v<typename std::variant<T, Ts...>>-1;

		if (std::uint16_t index = std::uint16_t(-1); ar.optional_attributes(optional_nvp(limited_arithmetic(index, 0, max_index), "index"))) {
			variant_detail::load(ar, variant, index, std::index_sequence_for<T, Ts...>());
		}
	}

	template <typename Archive, typename T, typename... Ts> inline
	std::enable_if_t<trait::is_textual_v<Archive>>
	save(Archive &ar, const with_type_as_index<std::variant<T, Ts...> &> &v)
	{
		std::visit([&ar](const auto &value){
			ar.attribute(get_type_name(ar, value), "type")(
				nvp(value, "")
			);
		}, v.value);
	}

	template <typename Archive, typename T, typename... Ts> inline
	std::enable_if_t<trait::is_textual_v<Archive>>
	load(Archive &ar, with_type_as_index<std::variant<T, Ts...> &> &v)
	{
		std::string type_name;

		if (ar.attribute(type_name, "type")) {
			if (!variant_detail::load(ar, v.value, type_name, std::index_sequence_for<T, Ts...>())) {
				ar.error(EINVAL);
			}
		}
	}

	template <typename Archive, typename T, typename... Ts> inline
	std::enable_if_t<!trait::is_textual_v<Archive>>
	save(Archive &ar, const with_type_as_index<std::variant<T, Ts...> &> &v)
	{
		save(ar, v.value);
	}

	template <typename Archive, typename T, typename... Ts> inline
	std::enable_if_t<!trait::is_textual_v<Archive>>
	load(Archive &ar, with_type_as_index<std::variant<T, Ts...> &> &v)
	{
		load(ar, v.value);
	}

	template <typename Archive, typename T, typename... Ts> inline
	void save(Archive &ar, const fz::covariant<T, Ts...> &variant)
	{
		save(ar, static_cast<const std::variant<T, Ts...>&>(variant));
	}

#if 0
	template <typename Archive, typename T, typename... Ts> inline
	// See below comment about why this is currently disabled for textual archives.
	std::enable_if_t<!trait::is_textual_v<Archive>>
	load(Archive &ar, fz::covariant<T, Ts...> &variant)
	{
		constexpr auto max_index = std::variant_size_v<typename std::variant<T, Ts...>>-1;

		if (std::uint16_t index = std::uint16_t(-1); ar.optional_attribute(index, "index")) {
			// Through slicing via the common base class, we will let at least some of the info pass through.
			// This assumes that the real type has been serialized so to allow for this de-serialization slicing.
			// This will be the case iff the base class is serialized from within the derived class via the
			// base_class() helper as its 1st serialization step, so that its serialized data will be first.
			// This poses an issue with textual-archives, in case of member names shared between derived and parent class.
			// Not sure how to deal with this yet, hence...
			// FIXME: find a way to deal with textual archives as depicted above.
			if (index > max_index)
				index = 0;

			variant_detail::load(ar, static_cast<std::variant<T, Ts...>&>(variant), index, std::index_sequence_for<T, Ts...>());
		}
	}
#endif

	template <typename Archive>
	void serialize(Archive &, std::monostate &) { }

}

#endif // FZ_SERIALIZATION_TYPES_VARIANT_HPP
