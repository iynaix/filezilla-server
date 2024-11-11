#ifndef FZ_SERIALIZATION_TYPES_CONTAINERS_HPP
#define FZ_SERIALIZATION_TYPES_CONTAINERS_HPP

#include <utility>
#include <cerrno>

#include <libfilezilla/forward_like.hpp>

#include "../../serialization/serialization.hpp"
#include "../../util/traits.hpp"

namespace fz::serialization {

	// Generic loaders and savers for classes like std::vector, std::list, std::string, etc.

	template <typename T, std::enable_if_t<util::is_like_map<std::decay_t<T>>::value>* = nullptr>
	class with_key_name_base
	{
	protected:
		T value_;

	public:
		using key_type = typename std::decay_t<T>::key_type;
		using mapped_type = typename std::decay_t<T>::mapped_type;
		using value_type = typename std::decay_t<T>::value_type;
		using size_type = typename std::decay_t<T>::size_type;


		using iterator = typename std::decay_t<T>::iterator;
		using const_iterator = typename std::decay_t<T>::const_iterator;

		with_key_name_base() = default;

		template <typename... Us, std::enable_if_t<std::is_constructible_v<T, Us...>>* = nullptr>
		with_key_name_base(Us &&... us)
			: value_(std::forward<Us>(us)...)
		{}

		template <typename U, std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>>>* = nullptr>
		with_key_name_base(const std::initializer_list<U> &list)
			: value_(list)
		{}

		template <typename... Ts>
		auto insert(Ts &&... vs) -> decltype(value_.insert(std::forward<Ts>(vs)...))
		{
			return value_.insert(std::forward<Ts>(vs)...);
		}

		template <typename... Ts>
		auto emplace(Ts &&... vs) -> decltype(value_.emplace(std::forward<Ts>(vs)...))
		{
			return value_.emplace(std::forward<Ts>(vs)...);
		}

		template <typename... Ts>
		auto try_emplace(Ts &&... vs) -> decltype(value_.try_emplace(std::forward<Ts>(vs)...))
		{
			return value_.try_emplace(std::forward<Ts>(vs)...);
		}

		template <typename K>
		auto find(K && k) -> decltype(value_.find(std::forward<K>(k)))
		{
			return value_.find(std::forward<K>(k));
		}

		template <typename K>
		auto find(K && k) const -> decltype(value_.find(std::forward<K>(k)))
		{
			return value_.find(std::forward<K>(k));
		}

		auto begin() -> decltype(value_.begin())
		{
			return value_.begin();
		}

		auto end() -> decltype(value_.end())
		{
			return value_.end();
		}

		auto begin() const -> decltype(value_.begin())
		{
			return value_.begin();
		}

		auto end() const -> decltype(value_.end())
		{
			return value_.end();
		}

		auto cbegin() const -> decltype(value_.cbegin())
		{
			return value_.cbegin();
		}

		auto cend() const -> decltype(value_.cend())
		{
			return value_.cend();
		}

		auto size() const -> decltype(value_.size())
		{
			return value_.size();
		}

		auto clear() -> decltype(value_.clear())
		{
			return value_.clear();
		}

		template <typename K>
		auto count(K && k) const -> decltype(value_.count(std::forward<K>(k)))
		{
			return value_.count(std::forward<K>(k));
		}

		template <typename K>
		auto operator[](K && k) -> decltype(value_.operator[](std::forward<K>(k)))
		{
			return value_.operator[](std::forward<K>(k));
		}

		template <typename K>
		auto operator[](K && k) const -> decltype(value_.operator[](std::forward<K>(k)))
		{
			return value_.operator[](std::forward<K>(k));
		}

	};

	template <typename T, const char *Name, bool = std::is_reference_v<T>>
	class with_key_name;

	template <typename T>
	class with_key_name<T, nullptr, false>
	{
		static_assert(util::delayed_assert<T>, "The Name template parameter must refer to a valid string");
	};

	template <typename T, const char *Name>
	class with_key_name<T, Name, false> : public with_key_name_base<T>
	{
	public:
		using with_key_name_base<T>::with_key_name_base;

		friend constexpr const char *key_name(const with_key_name &)
		{
			return Name;
		}
	};

	template <typename T>
	class with_key_name<T, nullptr, true>: public with_key_name_base<T>
	{
		const char *name_;

	public:
		constexpr with_key_name(T value, const char *name) noexcept
			: with_key_name_base<T>(std::forward<T>(value))
			, name_(name)
		{}

		friend constexpr const char *key_name(const with_key_name &self)
		{
			return self.name_;
		}
	};

	template <typename T, std::enable_if_t<util::is_like_map<std::decay_t<T>>::value>* = nullptr>
	with_key_name(T && v, const char *name) -> with_key_name<decltype(std::forward<T>(v)), nullptr>;

	template <typename T, std::enable_if_t<util::is_like_map<T>::value>* = nullptr>
	constexpr const char *key_name(const T&) noexcept
	{
		return "key";
	}

	namespace  containers_detail {

		template <typename Container, typename Value>
		decltype(auto) maybe_wrap_map_item(const Container &c, Value && v) {
			if constexpr (util::is_map_item<std::decay_t<Container>, std::decay_t<Value>>::value)
				return map_item(fz::forward_like<Value>(v.first), fz::forward_like<Value>(v.second), key_name(c));
			else
				return std::forward<Value>(v);
		}

		template <typename Container>
		auto maybe_make_map_item(const Container &c) {
			if constexpr (util::is_like_map_v<std::decay_t<Container>>)
				return map_item<std::decay_t<typename Container::key_type>, std::decay_t<typename Container::mapped_type>>({}, {}, key_name(c));
			else
				return std::decay_t<typename Container::value_type>{};
		}
	}

	/* Use these if the type is serializable as a blob of binary data and the Archive supports the size tag. */
	template <typename Archive, typename T, trait::enable_if<
		trait::is_serializable<Archive, binary_data<typename T::value_type>>::value &&
		trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::is_contiguous<T>::value
	> = trait::sfinae>
	void save(Archive &ar, const T &t)
	{
		ar(size_tag(t.size()), binary_data{t.data(), t.size()});
	}


	template <typename Archive, typename T, trait::enable_if<
		trait::is_serializable<Archive, binary_data<typename T::value_type>>::value &&
		trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::is_resizable<T>::value
	> = trait::sfinae>
	void load(Archive &ar, T &t)
	{
		if (std::size_t size; ar(size_tag(size))) {
			t.resize(size);

			ar(binary_data(t.data(), size));
		}
	}


	/* Use these if the type is NOT serializable as a blob of binary data and the Archive supports the size tag. */
	template <typename Archive, typename T, trait::enable_if<
		!trait::is_serializable<Archive, binary_data<typename T::value_type>>::value &&
		trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::has_iterators<T>::value &&
		util::has_size<T>::value
	> = trait::sfinae>
	void save(Archive &ar, const T &t)
	{
		if (ar(size_tag(t.size()))) {
			for (const auto &elem: t) {
				if (!ar(containers_detail::maybe_wrap_map_item(t, elem)))
					break;
			}
		}
	}

	template <typename Archive, typename T, trait::enable_if<
		!trait::is_serializable<Archive, binary_data<typename T::value_type>>::value &&
		trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::is_appendable<T>::value
	> = trait::sfinae>
	void load(Archive &ar, T &t)
	{
		if (std::size_t size; ar(size_tag(size))) {
			if constexpr(util::is_reservable<T>::value) {
				t.clear();
				t.reserve(size);
			}

			auto item = containers_detail::maybe_make_map_item(t);

			while (size-- && ar(item)) {
				t.insert(t.end(), std::move(item));

				item = containers_detail::maybe_make_map_item(t);
			}
		}
	}

	// Use these if the Archive does NOT support the size tag
	template <typename Archive, typename T, trait::enable_if<
		!trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::has_iterators<T>::value
	> = trait::sfinae>
	void save(Archive &ar, const T &t)
	{
		for (const auto &elem: t) {
			if (!ar(containers_detail::maybe_wrap_map_item(t, elem)))
				break;
		}
	}

	template <typename Archive, typename T, trait::enable_if<
		!trait::is_serializable<Archive, size_tag<typename T::size_type>>::value &&
		util::is_appendable<T>::value
	> = trait::sfinae>
	void load(Archive &ar, T &t)
	{
		decltype(auto) item = containers_detail::maybe_make_map_item(t);

		t.clear();

		while (ar(item)) {
			t.insert(t.end(), std::move(item));

			item = containers_detail::maybe_make_map_item(t);
		}

		// The archive is expected to error with ENOENT whenever the items in the container have been exhausted.
		// Any other error is fatal.
		if (ar.error() == ENOENT && ar.error_nesting_level() == ar.nesting_level()+1)
			ar.error(0);
	}

	template <typename T>
	struct unique;

	template <typename T>
	struct unique<T &>
	{
		unique(T &v)
			: v_(v)
		{}

		template <typename Archive>
		void save(Archive &ar)
		{
			ar(v_);
		}

		template <typename Archive>
		void load(Archive &ar)
		{
			ar(v_);

			std::sort(std::begin(v_), std::end(v_));
			v_.erase(std::unique(std::begin(v_), std::end(v_)), std::end(v_));
		}

		template <typename Archive>
		friend void prologue(Archive &, unique &) {}

		template <typename Archive>
		friend void epilogue(Archive &, unique &) {}

		T& v_;
	};

	template <typename T>
	unique(std::vector<T> &) -> unique<std::vector<T> &>;

	template <typename T>
	unique(const std::vector<T> &) -> unique<const std::vector<T> &>;
}

#endif // FZ_SERIALIZATION_TYPES_CONTAINERS_HPP
