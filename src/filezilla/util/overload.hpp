#ifndef FZ_UTIL_OVERLOAD_HPP
#define FZ_UTIL_OVERLOAD_HPP

namespace fz::util
{

// helper type for lambda visitors
template<typename... Ts> struct overload : Ts... { using Ts::operator()...; };
template<typename... Ts> overload(Ts...) -> overload<Ts...>;

}

#endif // FZ_UTIL_OVERLOAD_HPP
