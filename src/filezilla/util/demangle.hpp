#ifndef FZ_UTIL_DEMANGLE_HPP
#define FZ_UTIL_DEMANGLE_HPP

#include <string>
#include <typeinfo>

namespace fz::util {

std::string demangle(const char* name);

#if defined(FZ_UTIL_DEMANGLE_HAS_CONSTEXPR_TYPE_NAME) && FZ_UTIL_DEMANGLE_HAS_CONSTEXPR_TYPE_NAME
template <typename T>
constexpr std::string_view type_name() {
#ifdef _MSC_VER
	constexpr std::string_view name = __FUNCSIG__;
	constexpr auto start = name.find("get_name<") + 9;
	constexpr auto end = name.rfind(">(");
#else
	constexpr std::string_view name = __PRETTY_FUNCTION__;
	constexpr auto start = name.find("T = ") + 4;
	constexpr auto closing_brk = name.rfind("]");
	constexpr auto tmp = std::string_view(name.data()+start, name.length()-start);
	constexpr auto tmp_semicolon = tmp.find(";");
	constexpr auto semicolon = tmp_semicolon != name.npos ? tmp_semicolon + start : name.npos;
	constexpr auto end = closing_brk < semicolon ? closing_brk : semicolon;
#endif

	return name.substr(start, end - start);
}

template <typename T>
constexpr std::string_view type_name(const T &)
{
	return type_name<T>();
}

#else

template <typename T>
inline std::string type_name(const T &v)
{
	return demangle(typeid(v).name());
}

template <typename T>
inline std::string type_name()
{
	return demangle(typeid(T).name());
}
#endif

}

#endif // FZ_UTIL_DEMANGLE_HPP
