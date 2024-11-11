#ifndef FZ_UTIL_SERIALIZABLE_HPP
#define FZ_UTIL_SERIALIZABLE_HPP

#include <vector>
#include <array>
#include <memory>

#include "../serialization/types/optional.hpp"

namespace fz::util {

namespace serializable_detail {

	template <typename Archive>
	struct serializer
	{
		virtual void serialize(Archive &ar) = 0;
	};

	template <typename Derived, typename Interface, typename Archive>
	struct value_serializer: virtual Interface
	{
		using Interface::serialize;
		void serialize(Archive &ar) override final {
			static_cast<Derived*>(this)->impl(ar);
		}
	};

	struct resetter
	{
		virtual ~resetter() = default;
		virtual void reset() = 0;
	};

	struct empty {
		template <typename T>
		operator T(){ return {}; }
	};

}

template <typename Derived, typename... Archives>
struct serializable {
private:
	template <typename T, std::size_t NumNestedNames>
	struct intermediary_value_with_names  {
		serializable *owner_;
		T value_;
		std::array<const char *, 1+NumNestedNames> names_;
		const char *info_ = nullptr;

		intermediary_value_with_names &&info(const char *i) && { info_ = i; return std::move(*this); }
	};

	template <typename T>
	struct intermediary_value {
		serializable *owner_;
		T value_;

		template <typename... NestedNames, std::enable_if_t<(std::is_convertible_v<std::decay_t<NestedNames>, const char*> && ...)> * = nullptr>
		intermediary_value_with_names<T, sizeof...(NestedNames)> names(const char *name, const NestedNames &... nested_names) && {
			return { owner_, std::forward<T>(value_), {name, nested_names...} };
		}

		intermediary_value_with_names<T, 0> name(const char *n) && {
			return { owner_, std::forward<T>(value_), {n} };
		}
	};

	struct serializer: serializable_detail::serializer<Archives>...
	{
		virtual ~serializer() = default;
		using serializable_detail::serializer<Archives>::serialize...;
	};

	template <typename NVP>
	struct value_serializer final: serializable_detail::value_serializer<value_serializer<NVP>, serializer, Archives>... {
		value_serializer(NVP && nvp, const char *info)
			: nvp_(std::move(nvp))
			, info_(info)
		{}

		template <typename Archive>
		void impl(Archive &ar)
		{
			ar(serialization::value_info{nvp_, info_});
		}

		NVP nvp_;
		const char *info_;
	};

	template <typename T, typename = void>
	struct concrete_value {
		template <typename U, std::size_t N>
		concrete_value(intermediary_value_with_names<U, N> && fv)
			: value(fv.value_)
			, owner(fv.owner_)
		{
			owner->value_serializers_.emplace_back(new value_serializer<decltype(serialization::nvp{value, fv.names_})>{serialization::nvp{value, fv.names_}, fv.info_});
		}

		template <typename U>
		Derived &operator()(U && u) { value = std::forward<T>(u);  return static_cast<Derived&>(*owner); }
		T &operator()() { return value; }
		const T &operator()() const { return value; }

	protected:
		T value;
		serializable *owner;
	};

	template <typename T>
	struct concrete_value<std::optional<T>> {
		template <typename U, std::size_t N>
		concrete_value(intermediary_value_with_names<U, N> && fv)
			: value(fv.value_)
			, default_value(fv.value_)
			, owner(fv.owner_)
		{
			owner->value_serializers_.emplace_back(new value_serializer<decltype(serialization::nvp{value, fv.names_})>(serialization::nvp{value, fv.names_}, fv.info_));

			struct resetter: serializable_detail::resetter
			{
				void reset() override
				{
					v->reset();
				}

				resetter(concrete_value *v)
					: v(v)
				{}

			private:
				concrete_value *v;
			};

			owner->resetters_.emplace_back(new resetter(this));
		}

		bool has_value() const { return value || default_value; }
		explicit operator bool() const { return has_value(); }

		template <typename U>
		Derived & operator()(U && u) { value = std::forward<T>(u);  return static_cast<Derived&>(*owner); }
		Derived & operator()(std::nullopt_t) { value = default_value;  return static_cast<Derived&>(*owner); }

		T& operator()() {
			if (!value)
				value = default_value;

			return *value;
		}

		const T& operator()() const {
			if (!value)
				return *default_value;

			return *value;
		}

		void reset() { operator()(std::nullopt); }

	protected:
		std::optional<T> value;
		std::optional<T> default_value;
		serializable *owner;
	};

protected:
	template <typename T>
	struct mandatory: concrete_value<T> {
		using concrete_value<T>::concrete_value;

		static_assert(!util::is_optional_v<T>, "Please, use 'optional<T>' rather than 'mandatory<std::optional<T>>' type to define an optional field whose type is T.");
	};

	template <typename T>
	struct optional: concrete_value<std::optional<T>> {
		using concrete_value<std::optional<T>>::concrete_value;
	};

	template <typename T>
	intermediary_value<T&&> value(T && v) { return {this, std::forward<T>(v)}; }

	template <typename T>
	intermediary_value<std::initializer_list<T>&&> value(std::initializer_list<T> && v) { return {this, std::move(v)}; }

	intermediary_value<serializable_detail::empty> value(serializable_detail::empty) { return {this, {}}; }

	template <typename... NestedNames, std::enable_if_t<(std::is_convertible_v<std::decay_t<NestedNames>, const char*> && ...)> * = nullptr>
	auto names(const char *name, const NestedNames &... nested_names) {
		return value({}).names(name, nested_names...);
	}

	auto name(const char *n) {
		return value({}).name(n);
	}

public:
	template <typename Archive>
	void serialize(Archive &ar) {
		for (const auto &value: value_serializers_) {
			value->serialize(ar);
		}
	}

	void reset_optionals()
	{
		for (auto &r: resetters_)
			r->reset();
	}

private:
	std::vector<std::unique_ptr<serializer>> value_serializers_;
	std::vector<std::unique_ptr<serializable_detail::resetter>> resetters_;
};

}

#define FZ_UTIL_SERIALIZABLE_INIT_TEMPLATE(...) \
private: \
	using __fz_serializable = __VA_ARGS__; \
	using __fz_serializable::value; \
	using __fz_serializable::names; \
	using __fz_serializable::name; \
protected: \
	template <typename T> using mandatory = typename __fz_serializable::template mandatory<T>; \
	template <typename T> using optional = typename __fz_serializable::template optional<T>; \
/***/

#endif // FZ_UTIL_SERIALIZABLE_HPP
