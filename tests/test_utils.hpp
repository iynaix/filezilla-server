#ifndef FZ_TEST_UTILS_HEADER
#define FZ_TEST_UTILS_HEADER

#include <libfilezilla/string.hpp>

#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

template<typename T>
std::string inline value_to_string(T const& t, typename std::enable_if_t<std::is_enum_v<T>>* = nullptr)
{
	return std::to_string(std::underlying_type_t<T>(t));
}

template<typename T>
std::string inline value_to_string(T const& t, typename std::enable_if_t<std::is_arithmetic_v<T>>* = nullptr)
{
	return std::to_string(t);
}

std::string inline value_to_string(std::string const& t) {
	return t;
}

std::string inline value_to_string(std::wstring const& t) {
	return fz::to_string(t);
}

template<typename V, typename D>
void inline assert_equal_data(V const& expected, V const& actual, std::string const& func, D const& data, CppUnit::SourceLine line)
{
	if (expected != actual) {
		std::string loc = func;
		if (!data.empty()) {
			loc += " with " + value_to_string(data);
		}
		CppUnit::Asserter::failNotEqual(value_to_string(expected), value_to_string(actual), line, loc);
	}
}

namespace CppUnit {
	template<>
	struct assertion_traits<std::wstring>
	{
		static bool equal(const std::wstring &x, const std::wstring &y)
		{
			return x == y;
		}

		static std::string toString(const std::wstring &ws)
		{
			return fz::to_utf8(ws);
		}
	};

	template<>
	struct assertion_traits<std::wstring_view>
	{
		static bool equal(std::wstring_view x, std::wstring_view y)
		{
			return x == y;
		}

		static std::string toString(std::wstring_view ws)
		{
			return fz::to_utf8(ws);
		}
	};
}


#define ASSERT_EQUAL_DATA(expected, actual, data) assert_equal_data((expected), (actual), #actual, data, CPPUNIT_SOURCELINE())
#define ASSERT_EQUAL(expected, actual) assert_equal_data((expected), (actual), #actual, std::string(), CPPUNIT_SOURCELINE())

#endif
