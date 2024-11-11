#ifndef FZ_SERIALIZATION_HELPERS_HPP
#define FZ_SERIALIZATION_HELPERS_HPP

#include <type_traits>
#include <utility>
#include <string_view>
#include <array>
#include <cerrno>
#include <limits>

#include <libfilezilla/string.hpp>

#include "../serialization/trait.hpp"

namespace fz::serialization
{
	enum class size_type: std::uint32_t {};
	static constexpr size_type max_size { std::numeric_limits<std::underlying_type_t<size_type>>::max() };
	static constexpr size_type min_size { std::numeric_limits<std::underlying_type_t<size_type>>::min() };

	static_assert(std::is_unsigned_v<std::underlying_type_t<size_type>>, "size_type's underlying type must be unsigned.");
	static_assert(sizeof(std::underlying_type_t<size_type>) <= sizeof(std::size_t), "size_type's underlying type must not exceed size_t in size.");

	template <typename T>
	std::enable_if_t<std::is_integral_v<T>,
	bool> constexpr operator<(size_type lhs, T rhs) { return std::underlying_type_t<size_type>(lhs) < rhs; }

	template <typename T>
	std::enable_if_t<std::is_integral_v<T>,
	bool> constexpr operator<(T lhs, size_type rhs) { return lhs < std::underlying_type_t<size_type>(rhs); }

	template <typename T>
	std::enable_if_t<std::is_integral_v<T>,
	bool> constexpr operator>(size_type lhs, T rhs) { return std::underlying_type_t<size_type>(lhs) > rhs; }

	template <typename T>
	std::enable_if_t<std::is_integral_v<T>,
	bool> constexpr operator>(T lhs, size_type rhs) { return lhs > std::underlying_type_t<size_type>(rhs); }

	template <typename T>
	struct binary_data {
		explicit binary_data(T *data, std::size_t size): data(data), size(size) {}

		T *data;
		std::size_t size;
	};

	template <typename T>
	binary_data(const T *t, std::size_t size) -> binary_data<const T>;

	template <typename T>
	binary_data(T *t, std::size_t size) -> binary_data<T>;

	//! Use size_tag to wrap the size of the container you want to serialize
	template <typename Size>
	class size_tag
	{
		static_assert(std::is_integral_v<std::decay_t<Size>>);
		static_assert(!std::is_signed_v<std::decay_t<Size>>);

	public:
		size_tag &operator =(const size_tag &) = delete;
		explicit size_tag(Size size): value(size) {}

		Size value;
	};

	template <typename Size, std::enable_if_t<std::is_integral_v<Size>>* = nullptr>
	size_tag(Size &) -> size_tag<Size &>;

	template <typename Size, std::enable_if_t<std::is_integral_v<Size>>* = nullptr>
	size_tag(const Size &) -> size_tag<Size>;

	template <typename T, std::size_t NumNestedNames = 0>
	struct nvp {
		T && value;
		std::array<const char *, 1+NumNestedNames> names;

		constexpr nvp(nvp &&) noexcept = default;

		template <typename U>
		constexpr nvp(U && u, const decltype(names) &names) noexcept
			: value{std::forward<U>(u)}
			, names(names)
		{}

		template <typename U, typename... NestedNames, std::enable_if_t<(std::is_convertible_v<std::decay_t<NestedNames>, const char*> && ...)> * = nullptr>
		constexpr nvp(U && u, const char *name, const NestedNames &... nested_names) noexcept
			: value{std::forward<U>(u)}
			, names{name, nested_names...}
		{}
	};

	template <typename T, typename... NestedNames, std::enable_if_t<(std::is_convertible_v<std::decay_t<NestedNames>, const char*> && ...)> * = nullptr>
	nvp(T && t, const char *name, const NestedNames &... nested_names) -> nvp<T, sizeof...(NestedNames)>;

	template <typename T, std::size_t N, std::enable_if_t<(N>0)> * = nullptr>
	nvp(T && t, const std::array<const char *, N> &names) -> nvp<T, N-1>;

	#define FZ_NVP(value) ::fz::serialization::nvp(value, #value)
	#define FZ_NVP_N(value, ...) ::fz::serialization::nvp(value, #value, __VA_ARGS__)

	namespace trait
	{
		template <typename T>
		struct is_nvp: std::false_type {};

		template <typename T, std::size_t N>
		struct is_nvp<nvp<T, N>>: std::true_type {
			using value_type = T;
			static constexpr std::size_t num_nested_names = N;
		};

		template <typename T>
		inline constexpr bool is_nvp_v = is_nvp<T>::value;

		template <typename Archive, typename T>
		struct is_minimal_nvp: std::conjunction<
			is_nvp<T>, std::disjunction<
				std::is_arithmetic<std::decay_t<typename is_nvp<T>::value_type>>,
				is_string<std::decay_t<typename is_nvp<T>::value_type>>,
				has_minimal_serialization<Archive, std::decay_t<typename is_nvp<T>::value_type>>
			>
		> {};

		template <typename Archive, typename T>
		inline constexpr bool is_minimal_nvp_v = is_minimal_nvp<Archive, T>::value;
	}

	template <typename T, std::size_t NumNestedNames = 0>
	struct optional_nvp: nvp<T, NumNestedNames> {
		using nvp<T, NumNestedNames>::nvp;
	};

	template <typename T, typename... NestedNames, std::enable_if_t<(std::is_convertible_v<std::decay_t<NestedNames>, const char*> && ...)> * = nullptr>
	optional_nvp(T && t, const char *name, const NestedNames &... nested_names) -> optional_nvp<T, sizeof...(NestedNames)>;

	namespace trait
	{
		template <typename T>
		struct is_optional_nvp: std::false_type {};

		template <typename T, std::size_t N>
		struct is_optional_nvp<optional_nvp<T, N>>: std::true_type {
			using value_type = T;
			static constexpr std::size_t num_nested_names = N;
		};

		template <typename T>
		inline constexpr bool is_optional_nvp_v = is_optional_nvp<T>::value;

		template <typename Archive, typename T>
		struct is_minimal_optional_nvp: std::conjunction<
			is_optional_nvp<T>, std::disjunction<
				std::is_arithmetic<std::decay_t<typename is_nvp<T>::value_type>>,
				is_string<std::decay_t<typename is_nvp<T>::value_type>>,
				has_minimal_serialization<Archive, std::decay_t<typename is_nvp<T>::value_type>>
			>
		> {};

		template <typename Archive, typename T>
		inline constexpr bool is_minimal_optional_nvp_v = is_minimal_optional_nvp<Archive, T>::value;

	}

	#define FZ_NVP_O(value) ::fz::serialization::optional_nvp(value, #value)

	//! Attributes act like nvp's, but they can only be associated with integral or string types.
	//! Archives may treat attributes just like nvp's, or they may treat them especially.
	template <typename T>
	struct attribute: nvp<T> {
		using nvp<T>::nvp;

		attribute(nvp<T> && v)
			: nvp<T>(std::move(v))
		{}
	};

	template <typename T>
	attribute(T && t, const char *name) -> attribute<T>;

	namespace trait
	{
		template <typename T>
		struct is_attribute: std::false_type {};

		template <typename T>
		struct is_attribute<attribute<T>>: std::true_type {
			using value_type = T;
		};

		template <typename T>
		inline constexpr bool is_attribute_v = is_attribute<T>::value;
	}

	template <typename T>
	struct optional_attribute: optional_nvp<T> {
		using optional_nvp<T>::optional_nvp;

		optional_attribute(optional_nvp<T> && v)
			: optional_nvp<T>(std::move(v))
		{}
	};

	template <typename T>
	optional_attribute(T && t, const char *name) -> optional_attribute<T>;

	namespace trait
	{
		template <typename T>
		struct is_optional_attribute: std::false_type {};

		template <typename T>
		struct is_optional_attribute<optional_attribute<T>>: std::true_type {
			using value_type = T;
		};

		template <typename T>
		inline constexpr bool is_optional_attribute_v = is_optional_attribute<T>::value;
	}

	/// \brief Helpers to serialize the base class. Only available for non-virtual bases at the moment.
	///
	/// Use it this way:
	///
	/// template <typename Archive>
	/// void serialize(Archive &ar) {
	///     ar(base_class<Base>(this));
	///     /* ... rest of the serialization ... */
	/// }
	template <typename Base, typename Derived> std::enable_if_t<std::is_base_of_v<Base, Derived> && !util::is_virtual_base_of_v<Base, Derived>,
	nvp<Base &, 0>> base_class(Derived *derived)
	{
		return { static_cast<Base&>(*derived), ""};
	}

	template <typename Base, typename Derived> std::enable_if_t<std::is_base_of_v<Base, Derived> && !util::is_virtual_base_of_v<Base, Derived>,
	nvp<const Base &, 0>> base_class(const Derived *derived)
	{
		return { static_cast<const Base&>(*derived), ""};
	}

	template <typename Key, typename Value>
	struct map_item {
		map_item(Key key, Value value, const char *key_name)
			: key(std::forward<Key>(key))
			, value(std::forward<Value>(value))
			, key_name(key_name)
		{}

		template <typename K, typename V, std::enable_if_t<
			std::is_convertible_v<Key, K> && std::is_convertible_v<Value, V>
		>* = nullptr>
		operator std::pair<K, V>() const &
		{
			return {key, value};
		}

		template <typename K, typename V, std::enable_if_t<
			std::is_convertible_v<Key, K> && std::is_convertible_v<Value, V>
		>* = nullptr>
		operator std::pair<K, V>() &&
		{
			return {std::move(key), std::move(value)};
		}

		Key key;
		Value value;
		const char *key_name;
	};

	template <typename Key, typename Value>
	map_item(Key && key, Value && value, const char *key_name) -> map_item<decltype(std::forward<Key>(key)), decltype(std::forward<Value>(value))>;

	//! Provides textual info about the value being de/serialized.
	//! The archive may rightfully decide to ignore this info,
	//! but output archives might want to output it in such a way as to
	//! let the human user be able to understand more about the value itself.
	template <typename T>
	struct value_info {
		T && value;
		const char *info;

		template <typename U>
		value_info(U && value, const char *info)
			: value(std::forward<U>(value))
			, info(info)
		{}
	};

	template <typename T>
	value_info(T &&, const char *) -> value_info<T>;

	namespace trait
	{
		template <typename T>
		struct is_value_info: std::false_type {};

		template <typename T>
		struct is_value_info<value_info<T>>: std::true_type {
			using value_type = T;
		};

		template <typename T>
		inline constexpr bool is_value_info_v = is_value_info<T>::value;
	}

	#define FZ_NVP_I(value, info) ::fz::serialization::value_info(FZ_NVP(value), info)
	#define FZ_NVP_OI(value, info) ::fz::serialization::value_info(FZ_NVP_O(value), info)

	//! Limits the given arithmetic type T to the given range [min,max]
	template <typename T, std::enable_if_t<std::is_arithmetic_v<T>>* = nullptr>
	struct limited_arithmetic
	{
		T &value;
		T min;
		T max;

		template <typename Min, typename Max, std::enable_if_t<std::is_constructible_v<T, Min> && std::is_constructible_v<T, Max>>* = nullptr>
		limited_arithmetic(T &value, Min min, Max max)
			: value(value)
			, min(T(min))
			, max(T(max))
		{}

		template <typename Min, std::enable_if_t<std::is_constructible_v<T, Min>>* = nullptr>
		limited_arithmetic(T &value, Min min)
			: value(value)
			, min(T(min))
			, max(T(std::numeric_limits<T>::max()))
		{}

		template <typename Archive>
		void load_minimal(const Archive &ar, const T &v, typename Archive::error_t &error) {
			if (v < min || v > max)
				error = ar.error(ERANGE);

			value = v;
		}

		template <typename Archive>
		T save_minimal(const Archive &ar, typename Archive::error_t &error) {
			if (value < min || value > max)
				error = ar.error(ERANGE);

			return value;
		}
	};

	template <typename T, typename Min, typename Max, std::enable_if_t<std::is_constructible_v<T, Min> && std::is_constructible_v<T, Max> && std::is_arithmetic_v<T>>* = nullptr>
	limited_arithmetic(T &value, Min min, Max max) -> limited_arithmetic<T>;

	template <typename T, typename Min, std::enable_if_t<std::is_constructible_v<T, Min> && std::is_arithmetic_v<T>>* = nullptr>
	limited_arithmetic(T &value, Min min) -> limited_arithmetic<T>;

	//! Augments the textual serialization with the 'unlimited' word, aliased to a given value
	template <typename T, std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>* = nullptr>
	struct with_unlimited
	{
		T &value;
		T unlimited;

		template <typename U, std::enable_if_t<std::is_constructible_v<T, U>>* = nullptr>
		with_unlimited(T &value, U unlimited)
			: value(value)
			, unlimited(unlimited)
		{}

		template <typename Archive, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
		auto save_minimal(const Archive &) {
			if (value == unlimited)
				return std::string("unlimited");

			return std::to_string(value);
		}

		template <typename Archive, trait::enable_if<trait::is_textual_v<Archive>> = trait::sfinae>
		void load_minimal(const Archive &, const std::string &s) {
			if (std::string_view(s) == "unlimited")
				value = unlimited;
			else
				value = fz::to_integral<T>(s);
		}

		template <typename Archive, trait::enable_if<!trait::is_textual_v<Archive>> = trait::sfinae>
		auto save_minimal(const Archive &) {
			return value;
		}

		template <typename Archive, trait::enable_if<!trait::is_textual_v<Archive>> = trait::sfinae>
		void load_minimal(const Archive &, const T &v) {
			value = v;
		}
	};

	template <typename T, typename U, std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>* = nullptr>
	with_unlimited(T &value, U unlimited) -> with_unlimited<T>;

	template <typename T>
	struct with_type_as_index {
		T && value;

		template <typename U>
		with_type_as_index(U && value)
			: value(std::forward<U>(value))
		{}
	};

	template <typename T>
	with_type_as_index(T &&) -> with_type_as_index<T>;

	namespace trait
	{
		template <typename T>
		struct is_with_type_as_index: std::false_type {};

		template <typename T>
		struct is_with_type_as_index<with_type_as_index<T>>: std::true_type {
			using value_type = T;
		};

		template <typename T>
		inline constexpr bool is_with_type_as_index_v = is_with_type_as_index<T>::value;
	}
}

#endif // FZ_SERIALIZATION_HELPERS_HPP
