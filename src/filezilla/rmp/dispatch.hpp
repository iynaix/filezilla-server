#ifndef FZ_RMP_DISPATCH_HPP
#define FZ_RMP_DISPATCH_HPP

#include <libfilezilla/apply.hpp>

#include "../util/overload.hpp"

#include "any_message.hpp"

namespace fz::rmp {

namespace dispatch_detail {

	template <typename T, typename = void>
	struct has_is_success: std::false_type{};

	template <typename T>
	struct has_is_success<T, std::void_t<decltype(std::declval<T>().is_success() == true)>>: std::true_type{};

	template <typename T, typename H, typename F>
	struct visitor
	{
		visitor(H *h, const F &f)
			: h_(h)
			, f_(f)
		{}

		template <typename M, std::enable_if_t<std::is_same_v<std::decay_t<M>, std::decay_t<T>>>* = nullptr>
		bool operator()(M && m)
		{
			if constexpr (has_is_success<T>::value){
				std::apply([&](auto &&... ps){
					(h_->*f_)(m.is_success(), std::forward<decltype(ps)>(ps)...);
				}, std::forward<M>(m).tuple());
			}
			else {
				std::apply([&](auto &&... ps){
					(h_->*f_)(std::forward<decltype(ps)>(ps)...);
				}, std::forward<M>(m).tuple());
			}

			return true;
		}

	private:
		H *h_;
		const F &f_;
	};

	template <typename T, typename F>
	struct visitor<T, void, F>
	{
		visitor(const F &f)
			: f_(f)
		{}

		template <typename M, std::enable_if_t<std::is_same_v<std::decay_t<M>, std::decay_t<T>>>* = nullptr>
		bool operator()(M && m)
		{
			if constexpr (has_is_success<T>::value){
				std::apply([&](auto &&... ps){
					f_(m.is_success(), std::forward<decltype(ps)>(ps)...);
				}, std::forward<M>(m).tuple());
			}
			else {
				std::apply(f_, std::forward<M>(m).tuple());
			}

			return true;
		}

	private:
		const F &f_;
	};

}

template <typename... Ts, typename AnyMessage, typename H, typename... Fs>
constexpr auto dispatch(AnyMessage && any, H* h, const Fs &... fs) -> std::enable_if_t<trait::is_any_message<std::decay_t<AnyMessage>>::value, bool>
{
	return std::forward<AnyMessage>(any).visit(util::overload{
		dispatch_detail::visitor<Ts, H, Fs>(h, fs)...,
		[](auto &&) { return false; }
	});
}

template <typename... Ts, typename AnyMessage, typename... Fs>
constexpr auto dispatch(AnyMessage && any, const Fs &... fs) -> std::enable_if_t<trait::is_any_message<std::decay_t<AnyMessage>>::value, bool>
{
	return std::forward<AnyMessage>(any).visit(util::overload{
		dispatch_detail::visitor<Ts, void, Fs>(fs)...,
		[](auto &&) { return false; }
	});
}

}

#endif // FZ_RMP_DISPATCH_HPP
