#ifndef FZ_UTIL_SCOPE_GUARD_HPP
#define FZ_UTIL_SCOPE_GUARD_HPP

#include <utility>

#include "../preprocessor/cat.hpp"

namespace fz::util {

template<class F>
class scope_guard
{
public:
	scope_guard(F && f)
		: f_(std::forward<F>(f))
	{}

	scope_guard(const scope_guard &) = delete;
	scope_guard(scope_guard &&) = delete;
	scope_guard &operator=(const scope_guard &) = delete;
	scope_guard &operator=(scope_guard &&) = delete;

	~scope_guard()
	{
		f_();
	}

private:
	F f_;
};

template<class F>
scope_guard(F && f) -> scope_guard<F>;

#ifdef __COUNTER__
#	define FZ_SCOPE_GUARD ::fz::util::scope_guard FZ_PP_CAT(FZ_PP_CAT(fz_scope_guard_aux_var_,__COUNTER__),_) = [&]
#endif

}

#endif // FZ_UTIL_SCOPE_GUARD_HPP
