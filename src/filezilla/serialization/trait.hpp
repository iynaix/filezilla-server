#ifndef FZ_SERIALIZATION_TRAIT_HPP
#define FZ_SERIALIZATION_TRAIT_HPP

#include <type_traits>
#include <string>

#include "../util/traits.hpp"
#include "../serialization/archives/fwd.hpp"
#include "../serialization/version.hpp"
#include "../serialization/access.hpp"

namespace fz::serialization
{
	enum class specialization
	{
		unversioned_member_serialize,
		unversioned_member_load_save,
		unversioned_member_load_save_minimal,
		unversioned_member_load_save_minimal_with_error,
		unversioned_nonmember_serialize,
		unversioned_nonmember_load_save,
		unversioned_nonmember_load_save_minimal,
		unversioned_nonmember_load_save_minimal_with_error,

		versioned_member_serialize,
		versioned_member_load_save,
		versioned_member_load_save_minimal,
		versioned_member_load_save_minimal_with_error,
		versioned_nonmember_serialize,
		versioned_nonmember_load_save,
		versioned_nonmember_load_save_minimal,
		versioned_nonmember_load_save_minimal_with_error,
	};

	template <typename A, typename T, specialization S, typename SFINAEHelper = void>
	struct specialize: std::false_type{};

	#define FZ_SERIALIZATION_SPECIALIZE(Archive, Type, Specialization)                                                  \
		namespace fz::serialization { template <> struct specialize<Archive, Type, specialization::Specialization>{}; } \
	/***/

	#define FZ_SERIALIZATION_SPECIALIZE_ALL(Type, Specialization)                                                                       \
		namespace fz::serialization { template <typename Archive> struct specialize<Archive, Type, specialization::Specialization>{}; } \
	/***/

	template <typename A, typename T, typename SFINAEHelper = void>
	struct name_of_type;

	template <typename A, typename T>
	const std::string &get_type_name(A &, const T &)
	{
		return name_of_type<A, T>::value;
	}

	#define FZ_SERIALIZATION_NAME_OF_TYPE(Archive, Type, Name)                                                                            \
		namespace fz::serialization { template <> struct name_of_type<Archive, Type>{ static const inline std::string value{ Name }; }; } \
	/***/

	#define FZ_SERIALIZATION_NAME_OF_TYPE_ALL(Type, Name)                                                                                                 \
		namespace fz::serialization { template <typename Archive> struct name_of_type<Archive, Type>{ static const inline std::string value{ Name }; }; } \
	/***/
}

namespace fz::serialization::trait
{

	//***********//

	template <typename Archive>
	struct is_output: std::is_base_of<archive_is_output, Archive>{};

	template <typename Archive>
	struct is_input: std::is_base_of<archive_is_input, Archive>{};

	template <typename Archive>
	struct is_archive: std::disjunction<is_output<Archive>, is_input<Archive>>{};

	template <typename Archive>
	struct is_textual: std::is_base_of<archive_is_textual, Archive>{};

	//***********//

	template <typename OutputArchive>
	struct input_from_output {
		static_assert(is_output<OutputArchive>::value, "You must provide an Output archive as template parameter");
		static_assert(util::delayed_assert<OutputArchive>, "Link between input and output archives hasn't been made yet. Use FZ_SERIALIZATION_LINK_ARCHIVES macro in global namespace.");
	};

	template <typename InputArchive>
	struct output_from_input {
		static_assert(is_input<InputArchive>::value, "You must provide an Input archive as template parameter");
		static_assert(util::delayed_assert<InputArchive>, "Link between input and output archives hasn't been made yet. Use FZ_SERIALIZATION_LINK_ARCHIVES macro in global namespace.");
	};

	template <typename OutputArchive>
	using input_from_output_t = typename input_from_output<OutputArchive>::type;

	template <typename InputArchive>
	using output_from_input_t = typename output_from_input<InputArchive>::type;

	#define FZ_SERIALIZATION_LINK_ARCHIVES(Input, Output)                                                 \
		namespace fz::serialization::trait { template <> struct input_from_output<Output>{                \
			static_assert(is_output_v<Output>, "You must specify an output archive as second parameter"); \
			using type = Input;                                                                           \
		}; }                                                                                              \
		namespace fz::serialization::trait { template <> struct output_from_input<Input>{                 \
			static_assert(is_input_v<Input>, "You must specify an input archive as first parameter");     \
			using type = Output;                                                                          \
		}; }                                                                                              \
	/***/

	//***********//

	template <typename T>
	struct is_string: std::false_type {};

	template <typename ...Ts>
	struct is_string<std::basic_string<Ts...>>: std::true_type {};

	template <typename T>
	inline constexpr bool is_string_v = is_string<T>::value;

	//***********//

	template <typename Serializer, typename Archive, typename T, typename = void>
	struct has: std::false_type {};

	template <typename Serializer, typename Archive, typename T>
	inline constexpr bool has_v = has<Serializer, Archive, T>::value;

	#define FZ_HAS_TRAIT(versioned, member, name, test, type_assert)                                   \
		namespace versioned { namespace member { struct name{}; } }                                    \
		template <typename Archive, typename T>                                                        \
		struct has<versioned::member::name, Archive, T, std::void_t<decltype(test)>>: std::true_type { \
			using type = decltype(test);                                                               \
			static_assert type_assert(#versioned " " #member " " #name);                               \
		};                                                                                             \
	/***/

	#define FZ_VOID_ASSERT(name) (std::is_void_v<type>, "Return type of " name "() must be void.")
	#define FZ_ARITHMETIC_OR_STRING_ASSERT(name) (std::is_arithmetic_v<type> || std::is_enum_v<type> || is_string_v<type>, "Return type of " name "() must be either arithmetic or a string.")

	FZ_HAS_TRAIT(unversioned, member, serialize,               access::member_serialize(std::declval<Archive&>(), std::declval<std::decay_t<T>>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, save,                    access::member_save(std::declval<Archive&>(), std::declval<std::decay_t<T>>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, load,                    access::member_load(std::declval<Archive&>(), std::declval<std::decay_t<T>>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, save_minimal,            access::member_save_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, load_minimal,            access::member_load_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename has<unversioned::member::save_minimal, output_from_input_t<Archive>, T>::type>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, save_minimal_with_error, access::member_save_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename Archive::error_t&>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(unversioned, member, load_minimal_with_error, access::member_load_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename std::conditional_t<has_v<unversioned::member::save_minimal_with_error, output_from_input_t<Archive>, T>, has<unversioned::member::save_minimal_with_error, output_from_input_t<Archive>, T>, has<unversioned::member::save_minimal, output_from_input_t<Archive>, T>>::type>(), std::declval<typename Archive::error_t&>()), FZ_VOID_ASSERT)

	FZ_HAS_TRAIT(unversioned, nonmember, serialize,               serialize(std::declval<Archive&>(), std::declval<std::decay_t<T>&>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, save,                    save(std::declval<Archive&>(), std::declval<std::decay_t<T>&>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, load,                    load(std::declval<Archive&>(), std::declval<std::decay_t<T>&>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, save_minimal,            save_minimal(std::declval<const Archive&>(), std::declval<const std::decay_t<T>&>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, load_minimal,            load_minimal(std::declval<const Archive&>(), std::declval<T&>(), std::declval<typename has<unversioned::nonmember::save_minimal, output_from_input_t<Archive>, T>::type>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, save_minimal_with_error, save_minimal(std::declval<const Archive&>(), std::declval<const std::decay_t<T>&>(), std::declval<typename Archive::error_t&>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, load_minimal_with_error, load_minimal(std::declval<const Archive&>(), std::declval<std::decay_t<T>&>(), std::declval<typename std::conditional_t<has_v<unversioned::nonmember::save_minimal_with_error, output_from_input_t<Archive>, T>, has<unversioned::nonmember::save_minimal_with_error, output_from_input_t<Archive>, T>, has<unversioned::nonmember::save_minimal, output_from_input_t<Archive>, T>>::type>(), std::declval<typename Archive::error_t&>()), FZ_VOID_ASSERT)

	FZ_HAS_TRAIT(versioned, member, serialize,               access::member_serialize(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, member, save,                    access::member_save(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, member, load,                    access::member_load(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, member, save_minimal,            access::member_save_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<const version_of_t<T> &>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(versioned, member, load_minimal,            access::member_load_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename has<versioned::member::save_minimal, output_from_input_t<Archive>, T>::type>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, member, save_minimal_with_error, access::member_save_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename Archive::error_t&>(), std::declval<const version_of_t<T> &>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(versioned, member, load_minimal_with_error, access::member_load_minimal(std::declval<Archive&>(), std::declval<std::decay_t<T>>(), std::declval<typename std::conditional_t<has_v<versioned::member::save_minimal_with_error, output_from_input_t<Archive>, T>, has<versioned::member::save_minimal_with_error, output_from_input_t<Archive>, T>, has<versioned::member::save_minimal, output_from_input_t<Archive>, T>>::type>(), std::declval<typename Archive::error_t&>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)

	FZ_HAS_TRAIT(versioned, nonmember, serialize,               serialize(std::declval<Archive&>(), std::declval<std::decay_t<T>&>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, save,                    save(std::declval<Archive&>(), std::declval<std::decay_t<T>&>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, load,                    load(std::declval<Archive&>(), std::declval<std::decay_t<T>&>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, save_minimal,            save_minimal(std::declval<const Archive&>(), std::declval<const std::decay_t<T>&>(), std::declval<const version_of_t<T> &>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, load_minimal,            load_minimal(std::declval<const Archive&>(), std::declval<T&>(), std::declval<typename has<versioned::nonmember::save_minimal, output_from_input_t<Archive>, T>::type>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, save_minimal_with_error, save_minimal(std::declval<const Archive&>(), std::declval<const std::decay_t<T>&>(), std::declval<typename Archive::error_t&>(), std::declval<const version_of_t<T> &>()), FZ_ARITHMETIC_OR_STRING_ASSERT)
	FZ_HAS_TRAIT(versioned, nonmember, load_minimal_with_error, load_minimal(std::declval<const Archive&>(), std::declval<std::decay_t<T>&>(), std::declval<typename std::conditional_t<has_v<versioned::nonmember::save_minimal_with_error, output_from_input_t<Archive>, T>, has<versioned::nonmember::save_minimal_with_error, output_from_input_t<Archive>, T>, has<versioned::nonmember::save_minimal, output_from_input_t<Archive>, T>>::type>(), std::declval<typename Archive::error_t&>(), std::declval<const version_of_t<T> &>()), FZ_VOID_ASSERT)

	FZ_HAS_TRAIT(unversioned, nonmember, prologue, prologue(std::declval<Archive&>(), std::declval<const std::decay_t<T>&>()), FZ_VOID_ASSERT)
	FZ_HAS_TRAIT(unversioned, nonmember, epilogue, epilogue(std::declval<Archive&>(), std::declval<const std::decay_t<T>&>()), FZ_VOID_ASSERT)

	template <typename Archive, typename T>
	struct has_minimal_unversioned_input_serialization: std::disjunction<
		has<unversioned::member::load_minimal, Archive, T>,
		has<unversioned::nonmember::load_minimal, Archive, T>,
		has<unversioned::member::load_minimal_with_error, Archive, T>,
		has<unversioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct has_minimal_unversioned_output_serialization: std::disjunction<
		has<unversioned::member::save_minimal, Archive, T>,
		has<unversioned::nonmember::save_minimal, Archive, T>,
		has<unversioned::member::save_minimal_with_error, Archive, T>,
		has<unversioned::nonmember::save_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct has_minimal_versioned_input_serialization: std::disjunction<
		has<versioned::member::load_minimal, Archive, T>,
		has<versioned::nonmember::load_minimal, Archive, T>,
		has<versioned::member::load_minimal_with_error, Archive, T>,
		has<versioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct has_minimal_versioned_output_serialization: std::disjunction<
		has<unversioned::member::save_minimal, Archive, T>,
		has<unversioned::nonmember::save_minimal, Archive, T>,
		has<unversioned::member::save_minimal_with_error, Archive, T>,
		has<unversioned::nonmember::save_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct has_minimal_serialization: std::conditional_t<
		is_input<Archive>::value, std::disjunction<has_minimal_unversioned_input_serialization<Archive, T>, has_minimal_versioned_input_serialization<Archive, T>>, std::conditional_t<
		is_output<Archive>::value, std::disjunction<has_minimal_unversioned_output_serialization<Archive, T>, has_minimal_versioned_output_serialization<Archive, T>>,
		std::false_type
	>>{};

	#undef FZ_ARITHMETIC_OR_STRING_ASSERT
	#undef FZ_ARCHIVE_ASSERT
	#undef FZ_VOID_ASSERT
	#undef FZ_HAS_TRAIT

	template <typename Serializer, typename Archive, typename T, typename = std::true_type>                                                                        \
	struct is_specialized: std::false_type {};

	template <typename Serializer, typename Archive, typename T>
	inline constexpr bool is_specialized_v = is_specialized<Serializer, Archive, T>::value;

	#define FZ_IS_SPECIALIZED_TRAIT(versioned, member, name, specialization_name)                                  \
		template <typename Archive, typename T>                                                                    \
		struct is_specialized<versioned::member::name, Archive, T, typename                                        \
			std::negation<std::is_base_of<                                                                         \
				std::false_type,                                                                                   \
				specialize<Archive, std::decay_t<T>, specialization::versioned##_##member##_##specialization_name> \
			>>                                                                                                     \
		::type>: std::true_type {                                                                                  \
			static_assert(                                                                                         \
				has<versioned::member::name, Archive, T>::value,                                                              \
				"A specialization for " #versioned "_" #member "_" #specialization_name " has been set up, "       \
				"but the type has no proper " #versioned " " #member " " #name "()."                               \
			);                                                                                                     \
		};                                                                                                         \
	/***/

	#define FZ_IS_SPECIALIZED_TRAIT_ALL(name, specialization_name) \
		FZ_IS_SPECIALIZED_TRAIT(unversioned, member, name, specialization_name) \
		FZ_IS_SPECIALIZED_TRAIT(unversioned, nonmember, name, specialization_name) \
		FZ_IS_SPECIALIZED_TRAIT(versioned, member, name, specialization_name) \
		FZ_IS_SPECIALIZED_TRAIT(versioned, nonmember, name, specialization_name) \
	/***/

	FZ_IS_SPECIALIZED_TRAIT_ALL(serialize, serialize)
	FZ_IS_SPECIALIZED_TRAIT_ALL(load, load_save)
	FZ_IS_SPECIALIZED_TRAIT_ALL(save, load_save)
	FZ_IS_SPECIALIZED_TRAIT_ALL(load_minimal, load_save_minimal)
	FZ_IS_SPECIALIZED_TRAIT_ALL(save_minimal, load_save_minimal)
	FZ_IS_SPECIALIZED_TRAIT_ALL(load_minimal_with_error, load_save_minimal_with_error)
	FZ_IS_SPECIALIZED_TRAIT_ALL(save_minimal_with_error, load_save_minimal_with_error)

	#undef FZ_IS_SPECIALIZED_TRAIT

	template <typename Archive, typename T>
	struct count_unversioned_output_specializations: std::integral_constant<std::size_t,
		is_specialized_v<unversioned::member::serialize, Archive, T>               +
		is_specialized_v<unversioned::member::save, Archive, T>                    +
		is_specialized_v<unversioned::member::save_minimal, Archive, T>            +
		is_specialized_v<unversioned::member::save_minimal_with_error, Archive, T> +

		is_specialized_v<unversioned::nonmember::serialize, Archive, T>               +
		is_specialized_v<unversioned::nonmember::save, Archive, T>                    +
		is_specialized_v<unversioned::nonmember::save_minimal, Archive, T>            +
		is_specialized_v<unversioned::nonmember::save_minimal_with_error, Archive, T>
	 >{};

	template <typename Archive, typename T>
	struct count_versioned_output_specializations: std::integral_constant<std::size_t,
		is_specialized_v<versioned::member::serialize, Archive, T>               +
		is_specialized_v<versioned::member::save, Archive, T>                    +
		is_specialized_v<versioned::member::save_minimal, Archive, T>            +
		is_specialized_v<versioned::member::save_minimal_with_error, Archive, T> +

		is_specialized_v<versioned::nonmember::serialize, Archive, T>               +
		is_specialized_v<versioned::nonmember::save, Archive, T>                    +
		is_specialized_v<versioned::nonmember::save_minimal, Archive, T>            +
		is_specialized_v<versioned::nonmember::save_minimal_with_error, Archive, T>
	 >{};

	template <typename Archive, typename T>
	struct count_output_specializations: std::integral_constant<std::size_t,
		count_unversioned_output_specializations<Archive, T>::value +
		count_versioned_output_specializations<Archive, T>::value
	>{};

	template <typename Archive, typename T>
	struct count_unversioned_input_specializations: std::integral_constant<std::size_t,
		is_specialized_v<unversioned::member::serialize, Archive, T>               +
		is_specialized_v<unversioned::member::load, Archive, T>                    +
		is_specialized_v<unversioned::member::load_minimal, Archive, T>            +
		is_specialized_v<unversioned::member::load_minimal_with_error, Archive, T> +

		is_specialized_v<unversioned::nonmember::serialize, Archive, T>               +
		is_specialized_v<unversioned::nonmember::load, Archive, T>                    +
		is_specialized_v<unversioned::nonmember::load_minimal, Archive, T>            +
		is_specialized_v<unversioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_versioned_input_specializations: std::integral_constant<std::size_t,
		is_specialized_v<versioned::member::serialize, Archive, T>               +
		is_specialized_v<versioned::member::load, Archive, T>                    +
		is_specialized_v<versioned::member::load_minimal, Archive, T>            +
		is_specialized_v<versioned::member::load_minimal_with_error, Archive, T> +

		is_specialized_v<versioned::nonmember::serialize, Archive, T>               +
		is_specialized_v<versioned::nonmember::load, Archive, T>                    +
		is_specialized_v<versioned::nonmember::load_minimal, Archive, T>            +
		is_specialized_v<versioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_input_specializations: std::integral_constant<std::size_t,
		count_unversioned_input_specializations<Archive, T>::value +
		count_versioned_input_specializations<Archive, T>::value
	>{};

	template <typename Archive, typename T>
	struct count_unversioned_output_serializers: std::integral_constant<std::size_t,
		has_v<unversioned::member::serialize, Archive, T>               +
		has_v<unversioned::member::save, Archive, T>                    +
		has_v<unversioned::member::save_minimal, Archive, T>            +
		has_v<unversioned::member::save_minimal_with_error, Archive, T> +

		has_v<unversioned::nonmember::serialize, Archive, T>               +
		has_v<unversioned::nonmember::save, Archive, T>                    +
		has_v<unversioned::nonmember::save_minimal, Archive, T>            +
		has_v<unversioned::nonmember::save_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_versioned_output_serializers: std::integral_constant<std::size_t,
		has_v<versioned::member::serialize, Archive, T>               +
		has_v<versioned::member::save, Archive, T>                    +
		has_v<versioned::member::save_minimal, Archive, T>            +
		has_v<versioned::member::save_minimal_with_error, Archive, T> +

		has_v<versioned::nonmember::serialize, Archive, T>               +
		has_v<versioned::nonmember::save, Archive, T>                    +
		has_v<versioned::nonmember::save_minimal, Archive, T>            +
		has_v<versioned::nonmember::save_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_output_serializers: std::integral_constant<std::size_t,
		count_unversioned_output_serializers<Archive, T>::value +
		count_versioned_output_serializers<Archive, T>::value
	>{};

	template <typename Archive, typename T>
	struct count_unversioned_input_serializers: std::integral_constant<std::size_t,
		has_v<unversioned::member::serialize, Archive, T>               +
		has_v<unversioned::member::load, Archive, T>                    +
		has_v<unversioned::member::load_minimal, Archive, T>            +
		has_v<unversioned::member::load_minimal_with_error, Archive, T> +

		has_v<unversioned::nonmember::serialize, Archive, T>               +
		has_v<unversioned::nonmember::load, Archive, T>                    +
		has_v<unversioned::nonmember::load_minimal, Archive, T>            +
		has_v<unversioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_versioned_input_serializers: std::integral_constant<std::size_t,
		has_v<versioned::member::serialize, Archive, T>               +
		has_v<versioned::member::load, Archive, T>                    +
		has_v<versioned::member::load_minimal, Archive, T>            +
		has_v<versioned::member::load_minimal_with_error, Archive, T> +

		has_v<versioned::nonmember::serialize, Archive, T>               +
		has_v<versioned::nonmember::load, Archive, T>                    +
		has_v<versioned::nonmember::load_minimal, Archive, T>            +
		has_v<versioned::nonmember::load_minimal_with_error, Archive, T>
	>{};

	template <typename Archive, typename T>
	struct count_input_serializers: std::integral_constant<std::size_t,
		count_unversioned_input_serializers<Archive, T>::value +
		count_versioned_input_serializers<Archive, T>::value
	>{};

	#define FZ_DEFINE_PROCESS(member, name)                                                                                                             \
		static constexpr bool member##_##name =                                                                                                         \
			(has_v<versioned::member::name, Archive, T> && (is_specialized_v<versioned::member::name, Archive, T> || num_specializations == 0))     ||  \
			(has_v<unversioned::member::name, Archive, T> && (is_specialized_v<unversioned::member::name, Archive, T> || num_specializations == 0));
	/***/

	#define FZ_SERIALIZATION_ASSERT(versioned, member, name)                                                              \
		static_assert(has_v<versioned::member::name, Archive, T> == 0, #versioned " " #member " " #name "() is defined"); \
	/***/

	template <typename Archive, typename T>
	struct must_process_output {
	private:
		static constexpr auto num_specializations = count_output_specializations<Archive, T>::value;
		static constexpr auto num_serializers     = count_output_serializers<Archive, T>::value;

		static constexpr bool do_checks() {
			bool result = true;

			if constexpr (num_specializations > 1) {
				static_assert(util::delayed_assert<Archive, T>, "Only one specialization per (archive, type) pair is allowed.");
				result = false;
			}

			if constexpr (num_serializers == 0) {
				static_assert(util::delayed_assert<Archive, T>, "You must define a serialization function for the (archive, type) pair.");
				result = false;
			}
			else
			if constexpr (num_serializers > 1 && num_specializations == 0) {
				static_assert(util::delayed_assert<Archive, T>, "More than one serialization function is defined, without any specialization.");

				FZ_SERIALIZATION_ASSERT(unversioned, member, serialize)
				FZ_SERIALIZATION_ASSERT(unversioned, member, save)
				FZ_SERIALIZATION_ASSERT(unversioned, member, save_minimal)
				FZ_SERIALIZATION_ASSERT(unversioned, member, save_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, serialize)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, save)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, save_minimal)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, save_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(versioned, member, serialize)
				FZ_SERIALIZATION_ASSERT(versioned, member, save)
				FZ_SERIALIZATION_ASSERT(versioned, member, save_minimal)
				FZ_SERIALIZATION_ASSERT(versioned, member, save_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(versioned, nonmember, serialize)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, save)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, save_minimal)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, save_minimal_with_error)

				result = false;
			}

			return result;
		}

		static_assert(do_checks(), "Output serialization not possible");

	public:
		FZ_DEFINE_PROCESS(member, serialize)
		FZ_DEFINE_PROCESS(member, save)
		FZ_DEFINE_PROCESS(member, save_minimal)
		FZ_DEFINE_PROCESS(member, save_minimal_with_error)

		FZ_DEFINE_PROCESS(nonmember, serialize)
		FZ_DEFINE_PROCESS(nonmember, save)
		FZ_DEFINE_PROCESS(nonmember, save_minimal)
		FZ_DEFINE_PROCESS(nonmember, save_minimal_with_error)

		static constexpr bool versioned = count_versioned_output_serializers<Archive, T>::value > 0;
	};

	template <typename Archive, typename T>
	class must_process_input {
		static constexpr auto num_specializations = count_input_specializations<Archive, T>::value;
		static constexpr auto num_serializers     = count_input_serializers<Archive, T>::value;

		static constexpr bool do_checks() {
			bool result = true;

			if constexpr (num_specializations > 1) {
				static_assert(util::delayed_assert<Archive, T>, "Only one specialization per (archive, type) pair is allowed.");
				result = false;
			}

			if constexpr (num_serializers == 0) {
				static_assert(util::delayed_assert<Archive, T>, "You must define a serialization function for the (archive, type) pair.");
				result = false;
			}
			else
			if constexpr (num_serializers > 1 && num_specializations == 0) {
				static_assert(util::delayed_assert<Archive, T>, "More than one serialization function is defined, without any specialization.");

				FZ_SERIALIZATION_ASSERT(unversioned, member, serialize)
				FZ_SERIALIZATION_ASSERT(unversioned, member, load)
				FZ_SERIALIZATION_ASSERT(unversioned, member, load_minimal)
				FZ_SERIALIZATION_ASSERT(unversioned, member, load_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, serialize)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, load)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, load_minimal)
				FZ_SERIALIZATION_ASSERT(unversioned, nonmember, load_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(versioned, member, serialize)
				FZ_SERIALIZATION_ASSERT(versioned, member, load)
				FZ_SERIALIZATION_ASSERT(versioned, member, load_minimal)
				FZ_SERIALIZATION_ASSERT(versioned, member, load_minimal_with_error)

				FZ_SERIALIZATION_ASSERT(versioned, nonmember, serialize)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, load)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, load_minimal)
				FZ_SERIALIZATION_ASSERT(versioned, nonmember, load_minimal_with_error)

				result = false;
			}

			return result;
		}

		static_assert(do_checks(), "Input serialization not possible");

	public:
		FZ_DEFINE_PROCESS(member, serialize)
		FZ_DEFINE_PROCESS(member, load)
		FZ_DEFINE_PROCESS(member, load_minimal)
		FZ_DEFINE_PROCESS(member, load_minimal_with_error)

		FZ_DEFINE_PROCESS(nonmember, serialize)
		FZ_DEFINE_PROCESS(nonmember, load)
		FZ_DEFINE_PROCESS(nonmember, load_minimal)
		FZ_DEFINE_PROCESS(nonmember, load_minimal_with_error)

		static constexpr bool versioned = count_versioned_input_serializers<Archive, T>::value > 0;
	};

	#undef FZ_DEFINE_PROCESS
	#undef FZ_SERIALIZATION_ASSERT

	template <typename Archive, typename T>
	struct has_output_specializion: std::bool_constant<(count_output_specializations<Archive, T>::value > 0)> {};

	template <typename Archive, typename T>
	struct has_input_specializion: std::bool_constant<(count_input_specializations<Archive, T>::value > 0)> {};

	template <typename Archive, typename T>
	struct is_output_serializable: std::bool_constant<(count_output_serializers<Archive, T>::value > 0)> {};

	template <typename Archive, typename T>
	struct is_input_serializable: std::bool_constant<(count_input_serializers<Archive, T>::value > 0)> {};

	template <typename Archive, typename T>
	struct has_specializion: std::conditional_t<
		is_output<Archive>::value, has_output_specializion<Archive, T>, std::conditional_t<
		is_input<Archive>::value,  has_input_specializion<Archive, T>,
		std::false_type
	>>{};

	template <typename Archive, typename T>
	struct is_serializable: std::conditional_t<
		is_output<Archive>::value, is_output_serializable<Archive, T>, std::conditional_t<
		is_input<Archive>::value,  is_input_serializable<Archive, T>,
		std::false_type
	>>{};

	template <typename Archive>
	inline constexpr bool is_output_v = is_output<Archive>::value;

	template <typename Archive>
	inline constexpr bool is_input_v = is_input<Archive>::value;

	template <typename Archive>
	inline constexpr bool is_archive_v = is_archive<Archive>::value;

	template <typename Archive>
	inline constexpr bool is_textual_v = is_textual<Archive>::value;

	template <typename Archive, typename T>
	inline constexpr bool is_output_serializable_v = is_output_serializable<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool is_input_serializable_v = is_input_serializable<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool is_serializable_v = is_serializable<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool has_output_specializion_v = has_output_specializion<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool has_input_specialion_v = has_input_specializion<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool has_specializion_v = has_specializion<Archive, T>::value;

	template <typename Archive, typename T>
	inline constexpr bool has_minimal_serialization_v = has_minimal_serialization<Archive, T>::value;

	//***********//

	enum class sfinae_t {};

	template <bool Condition>
	using enable_if = std::enable_if_t<Condition, sfinae_t>;

	inline constexpr sfinae_t sfinae{};

	//***********//

	template <typename T, typename ...Ts>
	struct is_one_of: std::disjunction<std::is_base_of<Ts, T>...>{};

	template <typename T, typename ...Ts>
	inline constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

	//***********//

	template <typename Archive, typename... Archives>
	struct restrict: std::enable_if<is_one_of_v<Archive, Archives...>> {
		static_assert(std::conjunction_v<is_archive<Archives>...>, "Template parameters must be archives");
	};

	template <typename Archive, typename... Archives>
	using restrict_t = typename restrict<Archive, Archives...>::type;

}


#endif // FZ_SERIALIZATION_TRAIT_HPP
