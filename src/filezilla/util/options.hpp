#ifndef FZ_UTIL_OPTIONS_HPP
#define FZ_UTIL_OPTIONS_HPP

#include <utility>
#include <algorithm>
#include <libfilezilla/forward_like.hpp>

#include <libfilezilla/forward_like.hpp>

#include "../util/tuple_insert.hpp"
#include "../util/traits.hpp"

namespace fz::util
{

	template <typename Holder, typename ClassToConstruct = void, std::size_t OptionsPosition = last_position>
	class options;

	template <typename Holder>
	class options<Holder> {
		template <typename Value>
		struct created_opt {
			Value value_;
			Holder &holder_;
		};

	public:
		template <typename Value>
		struct opt {
			opt(const opt & rhs)
				: value_(rhs.value_)
				, offset_(rhs.offset_)
			{}

			opt(opt && rhs)
				: value_(std::move(rhs.value_))
				, offset_(rhs.offset_)
			{}

			opt& operator=(opt && rhs)
			{
				value_ = std::move(rhs.value_);
				return *this;
			}

			opt& operator=(const opt & rhs)
			{
				value_ = std::move(rhs.value_);
				return *this;
			}

			template <typename V = Value, std::enable_if_t<
				std::is_assignable_v<V&, std::initializer_list<typename V::value_type>>>* = nullptr>
			Holder &operator()(std::initializer_list<typename V::value_type> list) &
			{
				value_ = list;

				return *reinterpret_cast<Holder *>(reinterpret_cast<char *>(this) - offset_);
			}

			template <typename V = Value, std::enable_if_t<
				std::is_assignable_v<V&, std::initializer_list<typename V::value_type>>>* = nullptr>
			Holder &&operator()(std::initializer_list<typename V::value_type> list) &&
			{
				value_ = list;
				return std::move(*reinterpret_cast<Holder *>(reinterpret_cast<char *>(this) - offset_));
			}

			template <typename T, std::enable_if_t<std::is_assignable_v<Value&, T>>* = nullptr>
			Holder &operator()(T && value) &
			{
				value_ = std::forward<T>(value);
				return *reinterpret_cast<Holder *>(reinterpret_cast<char *>(this) - offset_);
			}

			template <typename T, std::enable_if_t<std::is_assignable_v<Value&, T>>* = nullptr>
			Holder &&operator()(T && value) &&
			{
				value_ = std::forward<T>(value);
				return std::move(*reinterpret_cast<Holder *>(reinterpret_cast<char *>(this) - offset_));
			}

			const Value &operator()() const { return value_; }
			Value &operator()() { return value_; }

		private:
			template <typename>
			friend struct opt;

			friend Holder;

			template <typename U, std::enable_if_t<std::is_convertible_v<U, Value>>* = nullptr>
			opt(created_opt<U> && rhs)
				: value_(forward_like<U>(std::move(rhs).value_))
				, offset_(reinterpret_cast<char *>(this) - reinterpret_cast<char *>(&rhs.holder_))
			{}

			template <typename U, std::enable_if_t<util::is_tuple_v<std::decay_t<U>>>* = nullptr>
			opt(created_opt<U> && rhs)
				: value_(std::make_from_tuple<Value>(forward_like<U>(std::move(rhs).value_)))
				, offset_(reinterpret_cast<char *>(this) - reinterpret_cast<char *>(&rhs.holder_))
			{}

			Value value_;
			std::ptrdiff_t offset_;
		};

	protected:
		struct empty {
			template <typename T>
			operator T() && {
				return {};
			}
		};

		// Account for brace-initialization of container types.
		template <typename T>
		created_opt<std::initializer_list<T>> o(std::initializer_list<T> list)
		{
			return {list, static_cast<Holder&>(*this)};
		}

		template <typename Value>
		created_opt<Value> o(Value && value) {
			return {std::forward<Value>(value), static_cast<Holder&>(*this)};
		}

		created_opt<empty> o() {
			return {empty{}, static_cast<Holder&>(*this)};
		}

		template <typename... Ts, std::enable_if<(sizeof...(Ts) > 1)>* = nullptr>
		created_opt<std::tuple<Ts...>> o(Ts &&... vs) {
			return { std::forward_as_tuple(std::forward<Ts>(vs)...) };
		}
	};

	template <typename Holder, typename ClassToConstruct, std::size_t OptionsPosition>
	class options: public options<Holder>
	{
	public:
		/// This will be hidden if the user defines an option named 'construct' in the Holder class.
		/// In that case, just use the call operator defined below.
		template <typename... Ts>
		ClassToConstruct construct(Ts &&... ts) {
			constexpr auto pos = OptionsPosition == last_position ? sizeof...(Ts) : OptionsPosition;

			return construct_impl(
				std::forward_as_tuple(std::forward<Ts>(ts)...),
				std::make_index_sequence<pos>(),
				std::make_index_sequence<sizeof...(Ts) - pos>()
			);
		}

		template <typename... Ts>
		ClassToConstruct operator()(Ts &&... ts) {
			return construct(std::forward<Ts>(ts)...);
		}

	private:
		template <typename Tuple, std::size_t... LIs, std::size_t... RIs>
		ClassToConstruct construct_impl(Tuple && tuple, std::index_sequence<LIs...>, std::index_sequence<RIs...>) {
			return ClassToConstruct(
				std::get<LIs>(std::forward<Tuple>(tuple))...,
				static_cast<Holder &>(*this),
				std::get<sizeof...(LIs) + RIs>(std::forward<Tuple>(tuple))...
			);
		}
	};

}

#define FZ_UTIL_OPTIONS_INIT_TEMPLATE(Holder, ClassToConstruct)                        \
	template <typename T>                                                              \
	using opt = typename fz::util::options<Holder, ClassToConstruct>::template opt<T>; \
	using fz::util::options<Holder, ClassToConstruct>::o;                              \
/***/

#endif // FZ_UTIL_OPTIONS_H
