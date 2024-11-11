#ifndef FZ_UTIL_TRAITS_HPP
#define FZ_UTIL_TRAITS_HPP

#include <type_traits>
#include <optional>
#include <variant>

#include "../preprocessor/expand.hpp"
#include "../preprocessor/identity.hpp"
#include "../mpl/identity.hpp"
#include "../mpl/index_of.hpp"
#include "../mpl/contains.hpp"

namespace fz::util {

	template <typename ...T>
	inline constexpr bool delayed_assert = false;

	template <typename T>
	struct is_tuple: std::false_type{};

	template <typename... Args>
	struct is_tuple<std::tuple<Args...>>: std::true_type{};

	template <typename T>
	struct is_like_tuple: std::false_type{};

	template <typename... Args, template <typename ...> typename Tuple>
	struct is_like_tuple<Tuple<Args...>>: std::true_type{
		template <typename ...Ts>
		using tuple = Tuple<Ts...>;
	};

	template <typename T>
	struct is_like_array: std::false_type{};

	template <typename T, std::size_t N, template <typename, std::size_t> typename Array>
	struct is_like_array<Array<T,N>>: std::true_type{
		template <typename T2, std::size_t N2>
		using array = Array<T2,N2>;
	};

	template <typename T>
	struct is_template: std::false_type{};

	template <template <typename...> typename T, typename... As>
	struct is_template<T<As...>>: std::true_type{};

	template <typename T>
	struct is_optional: std::false_type{};

	template <typename T>
	struct is_optional<std::optional<T>>: std::true_type{};

	template <typename Derived, typename... Archives>
	struct serializable;

	template <typename Derived, typename... Archives>
	std::true_type is_serializable_f(const ::fz::util::serializable<Derived, Archives...> &);
	std::false_type is_serializable_f(...);

	template <typename T>
	struct is_serializable: decltype(is_serializable_f(std::declval<T>())){};

	template <typename T, typename = void>
	struct has_call_member: std::false_type {};

	template <typename T>
	struct has_call_member<T, std::void_t<decltype(&T::operator())>>: std::true_type {};

	template <typename T, typename... Args>
	struct is_uniquely_invocable: std::conjunction<
			std::disjunction<
				std::negation<std::is_class<T>>,
				has_call_member<T>
			>,
			std::is_invocable<T, Args...>
	 > {};

	template <typename T, typename Tuple>
	struct has_type: mpl::contains<Tuple, T>::type
	{ };

	template <typename T, typename Tuple>
	struct type_index: mpl::index_of<Tuple, T>::type
	{ };

	template <typename From, typename To, typename = void>
	struct can_static_cast: std::false_type{};

	template <typename From, typename To>
	struct can_static_cast<From, To, std::void_t<decltype(static_cast<To>(std::declval<From>()))>>: std::true_type{};

	template <typename Base, typename Derived>
	struct is_virtual_base_of: std::conjunction<
		std::is_base_of<Base, Derived>,
		std::negation<can_static_cast<Base*, Derived*>>
	>{};

	template <typename U>
	struct underlying: std::conditional_t<
		std::is_enum_v<U>,
		std::underlying_type<U>,
		mpl::identity<U>
	>{};

	template <typename U>
	using underlying_t = typename underlying<U>::type;

	template <typename U, typename T>
	struct is_same_underlying: std::is_same<underlying<U>, underlying<T>>{};

	template <typename Container, typename = void>
	struct is_contiguous: std::false_type {};

	template <typename Container>
	struct is_contiguous<Container, std::void_t<decltype(std::declval<Container>().data(), std::declval<Container>().size())>>: std::true_type {};

	template <typename Container, typename = void>
	struct is_resizable: std::false_type {};

	template <typename Container>
	struct is_resizable<Container, std::void_t<decltype(std::declval<Container>().resize(std::declval<Container>().size()))>>: std::true_type {};

	template <typename Container, typename = void>
	struct is_reservable: std::false_type {};

	template <typename Container>
	struct is_reservable<Container, std::void_t<decltype(std::declval<Container>().reserve(std::declval<Container>().size()))>>: std::true_type {};

	template <typename Container, typename = void>
	struct is_appendable: std::false_type {};

	template <typename Container>
	struct is_appendable<Container, std::void_t<decltype(std::declval<Container>().insert(std::end(std::declval<Container>()), *std::begin(std::declval<Container>())))>>: std::true_type {};

	template <typename Container, typename = void>
	struct has_iterators: std::false_type {};

	template <typename Container>
	struct has_iterators<Container, std::void_t<decltype(std::begin(std::declval<Container>()), std::end(std::declval<Container>()))>>: std::true_type {};

	template <typename Container, typename = void>
	struct has_size: std::false_type {};

	template <typename Container>
	struct has_size<Container, std::void_t<decltype(std::declval<Container>().size())>>: std::true_type {};

	template <typename Container, typename = void>
	struct is_like_map: std::false_type {};

	template <typename Container>
	struct is_like_map<Container, std::void_t<typename Container::key_type, typename Container::mapped_type, typename Container::value_type>>: std::true_type {};

	template <typename Container, typename Value>
	struct is_map_item: std::conjunction<
		is_like_map<Container>,
		std::is_same<typename Container::value_type, Value>
	>{};

	template <typename T, typename = void>
	struct has_unlimited: std::false_type {};

	template <typename T>
	struct has_unlimited<T, std::void_t<decltype(T::unlimited)>> {};

	template <typename T>
	inline constexpr bool has_unlimited_v = has_unlimited<T>::value;

	template <typename T>
	inline constexpr bool is_like_map_v = is_like_map<T>::value;

	template <typename T>
	inline constexpr bool is_tuple_v = is_tuple<T>::value;

	template <typename T>
	inline constexpr bool is_like_tuple_v = is_like_tuple<T>::value;

	template <typename T>
	inline constexpr bool is_like_array_v = is_like_array<T>::value;

	template <typename T>
	inline constexpr bool is_template_v = is_template<T>::value;

	template <typename T>
	inline constexpr bool is_optional_v = is_optional<T>::value;

	template <typename T>
	inline constexpr bool is_serializable_v = is_serializable<T>::value;

	template <typename T>
	inline constexpr bool is_uniquely_invocable_v = is_uniquely_invocable<T>::value;

	template <typename T, typename Tuple>
	inline constexpr bool has_type_v = has_type<T, Tuple>::value;

	template <typename T, typename Tuple>
	inline constexpr std::size_t type_index_v = type_index<T, Tuple>::value;

	template <typename From, typename To>
	inline constexpr bool can_static_cast_v = can_static_cast<From, To>::value;

	template <typename Base, typename Derived>
	inline constexpr bool is_virtual_base_of_v = is_virtual_base_of<Base, Derived>::value;

	template <typename U, typename T>
	inline constexpr bool is_same_underlying_v = is_same_underlying<U, T>::value;

	template <typename T>
	inline constexpr bool is_contiguous_v = is_contiguous<T>::value;

	/// Macro to define a access_<class> and a is_a_<class> templates to determine
	/// wether a given class is in is-a relation with the *template* class <class>
	#define FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_NAME_AND_ARGS(trait_name, class_name, tmpl_args, tmpl_use_args)  \
		namespace trait {                                                                                             \
																													  \
			namespace detail {                                                                                        \
																													  \
				template <FZ_PP_EXPAND(FZ_PP_IDENTITY tmpl_args)>                                                     \
				class_name<FZ_PP_EXPAND(FZ_PP_IDENTITY tmpl_use_args)> access_##trait_name##_f(                       \
					const class_name<FZ_PP_EXPAND(FZ_PP_IDENTITY tmpl_use_args)> &                                    \
				);                                                                                                    \
			}                                                                                                         \
																													  \
			template <typename T>                                                                                     \
			using access_##trait_name = decltype(detail::access_##trait_name##_f(std::declval<T>()));                 \
																													  \
			template <typename T, typename = void>                                                                    \
			struct is_a_##trait_name: std::false_type{};                                                              \
																													  \
			template <typename T>                                                                                     \
			struct is_a_##trait_name<T, std::void_t<access_##trait_name<T>>>: std::true_type{};                       \
																													  \
			template <typename T>                                                                                     \
			struct is_##trait_name: std::false_type{};                                                                \
																													  \
			template <FZ_PP_EXPAND(FZ_PP_IDENTITY tmpl_args)>                                                         \
			struct is_##trait_name<class_name<FZ_PP_EXPAND(FZ_PP_IDENTITY tmpl_use_args)>>: std::true_type{};         \
																													  \
			template <typename T>                                                                                     \
			inline constexpr bool is_a_##trait_name##_v = is_a_##trait_name<T>::value;                                \
																													  \
			template <typename T>                                                                                     \
			inline constexpr bool is_##trait_name##_v = is_##trait_name<T>::value;                                    \
		}                                                                                                             \
	/***/

	#define FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_ARGS(class_name, tmpl_args, tmpl_use_args)                  \
		FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_NAME_AND_ARGS(class_name, class_name, tmpl_args, tmpl_use_args) \
	/***/


	#define FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(class_name)                                   \
		FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE_WITH_ARGS(class_name, (typename... Ts), (Ts...))  \
	/***/

}
#endif // FZ_UTIL_TRAITS_HPP
