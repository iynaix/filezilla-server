#ifndef FZ_MPL_REMOVE_HPP
#define FZ_MPL_REMOVE_HPP

#include <type_traits>
#include "append.hpp"
#include "fold.hpp"
#include "if.hpp"
#include "identity.hpp"

namespace fz::mpl {

template <typename Seq, template <typename> typename Pred>
struct remove_if;

template <template<typename...> typename Seq, typename ...Ts, template <typename> typename Pred>
struct remove_if<Seq<Ts...>, Pred> {
	template <typename NewSeq, typename T>
	using op = if_<
		Pred<T>,
		identity<NewSeq>,
		append<NewSeq, T>
	>;

	using type = fold_t<Seq<Ts...>, Seq<>, op>;
};

template <typename Seq, template <typename> typename Pred>
using remove_if_t = typename remove_if<Seq, Pred>::type;

template <typename Seq, typename T>
struct remove {
	template <typename SeqT>
	using op = std::is_same<SeqT, T>;

	using type = remove_if_t<Seq, op>;
};

template <typename Seq, typename T>
using remove_t = typename remove<Seq, T>::type;

}
#endif // FZ_MPL_REMOVE_HPP
