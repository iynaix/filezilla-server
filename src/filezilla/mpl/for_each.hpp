#ifndef FZ_MPL_FOR_EACH_HPP
#define FZ_MPL_FOR_EACH_HPP

#include "arity.hpp"
#include "at.hpp"
#include "identity.hpp"
#include "size_t.hpp"

namespace fz::mpl {

namespace detail {

	template <typename Seq, typename F, std::size_t... Is>
	auto for_each(F && f, const std::index_sequence<Is...>&)
	{
		// If F returns a boolean, stop iterating if the return is false.
		if constexpr ((std::is_invocable_r_v<bool, F, identity<typename at_c<Seq, Is>::type>> && ...)) {
			return !(std::forward<F>(f)(identity<typename at_c<Seq, Is>::type>{}) && ...);
		}
		else
		if constexpr ((std::is_invocable_r_v<bool, F, identity<typename at_c<Seq, Is>::type>, mpl::size_t_<Is>> && ...)) {
			return !(std::forward<F>(f)(identity<typename at_c<Seq, Is>::type>{}, mpl::size_t_<Is>()) && ...);
		}
		else
		if constexpr ((std::is_invocable_r_v<void, F, identity<typename at_c<Seq, Is>::type>, mpl::size_t_<Is>> && ...)) {
			(std::forward<F>(f)(identity<typename at_c<Seq, Is>::type>{}, mpl::size_t_<Is>()), ...);
		}
		else {
			(std::forward<F>(f)(identity<typename at_c<Seq, Is>::type>{}), ...);
		}
	}

	template <typename Seq, typename F, std::size_t... Is>
	void for_each_v(Seq && s, F && f, const std::index_sequence<Is...>&)
	{
		// If F returns a boolean, stop iterating if the return is false.
		if constexpr ((std::is_invocable_r_v<bool, F, decltype(std::get<Is>(std::forward<Seq>(s)))> && ...))
			(std::forward<F>(f)(std::get<Is>(std::forward<Seq>(s))) && ...);
		else
			(std::forward<F>(f)(std::get<Is>(std::forward<Seq>(s))), ...);
	}

}

template <typename Seq, typename F>
void for_each(F && f)
{
	detail::for_each<Seq>(std::forward<F>(f), typename arity<Seq>::indices());
}

template <typename Seq, typename F>
void for_each_v(Seq && s, F && f)
{
	detail::for_each_v(std::forward<Seq>(s), std::forward<F>(f), typename arity<std::decay_t<Seq>>::indices());
}

}


#endif // FZ_MPL_FOR_EACH_HPP
