#include <utility>

#include <libfilezilla/util.hpp>
#include <libfilezilla/encode.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/recursive_remove.hpp>
#include <libfilezilla/format.hpp>

#ifdef FZ_WINDOWS
#	include <fileapi.h>
#else
#	include <unistd.h>
#endif

#include "../src/filezilla/util/filesystem.hpp"

#include "test_utils.hpp"

/*
 * This testsuite asserts the correctness of the basic_path class.
 */


using namespace fz::util::fs;

class basic_path_test final : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(basic_path_test);
	CPPUNIT_TEST(test_normalize<std::string>);
	CPPUNIT_TEST(test_normalize<std::wstring>);
	CPPUNIT_TEST(test_append<std::string>);
	CPPUNIT_TEST(test_append<std::wstring>);
	CPPUNIT_TEST(test_base<std::string>);
	CPPUNIT_TEST(test_base<std::wstring>);
	CPPUNIT_TEST_SUITE_END();

public:
	template <typename String>
	void test_normalize();

	template <typename String>
	void test_append();

	template <typename Char, path_format Format, path_kind Kind>
	void test_normalize(std::size_t line, const std::basic_string<Char> &path, const std::basic_string<Char> &expected);

	template <typename String>
	void test_base();
};

CPPUNIT_TEST_SUITE_REGISTRATION(basic_path_test);

template <typename Char, path_format Format, path_kind Kind>
void basic_path_test::test_normalize(std::size_t line, const std::basic_string<Char> &path, const std::basic_string<Char> &expected)
{
	std::string explanation = fz::sprintf("Original: %s\n- Format  : %s\n- Kind    : %s\n- Line    : %d %s",
										  path,
										  Format == unix_format ? "unix" : "windows",
										  Kind == absolute_kind ? "absolute" : Kind == relative_kind ? "relative" : "any",
										  line, __FILE__);

	auto p = basic_path<Char, Format, Kind>(path, Format);

	CPPUNIT_ASSERT_EQUAL_MESSAGE(explanation, expected, p.str());
}

template <typename String>
void basic_path_test::test_normalize()
{
	using Char = typename String::value_type;
	#define S(string) fzS(Char, string)
	#define TEST(path, expected_unix, expected_unix_any_kind, expected_windows, expected_windows_any_kind)                        \
		test{ __LINE__, S(path), S(expected_unix), S(expected_unix_any_kind), S(expected_windows), S(expected_windows_any_kind) } \
	/***/
	#define TEST2(path, expected_unix, expected_windows)                                                        \
		test{ __LINE__, S(path), S(expected_unix), S(expected_unix), S(expected_windows), S(expected_windows) } \
	/***/

	struct test {
		std::size_t line;
		String path;
		String expected_unix, expected_unix_any_kind;
		String expected_windows, expected_windows_any_kind;
	};

	auto absolute_unix_tests = std::vector<test> {
		TEST("/..//./1/2/../4///", // Path to normalize
			 "/1/4", "/1/4",       // Expected result on unix, also as any_kind
			 "", "\\1\\4"          // Expected result on windows, also as any_kind
			)
	};

	auto relative_unix_tests = std::vector<test> {
		// In relative paths we don't remove the ..'s at the beginning.
		TEST2("../.././1/../2/./", // Path to normalize
			 "../../2",            // Expected result on unix, also as any_kind
			 "..\\..\\2"          // Expected result on windows, also as any_kind
			 ),

		// When a relative path reduces to a empty string, the normalized version becomes the dot.
		// This also tests that trailing slashes are removed.
		TEST2("foo/../bar/../baz/..//",
			 ".",
			 ".")
	};

	auto absolute_windows_tests = std::vector<test> {
		TEST2("//server/share/\\\\/1/./../2\\3/..\\4/5\\\\", // Path to normalize
			 "/server/share/\\\\/2\\3/..\\4/5\\\\",          // Expected result on unix as any_kind
			 "\\\\server\\share\\2\\4\\5"                    // Expected result on windows, also as any_kind
			 ),

		TEST("C://\\\\/1/../2\\3/..\\4/./5\\\\",
			  "", "C:/\\\\/2\\3/..\\4/5\\\\",
			  "C:\\2\\4\\5", "C:\\2\\4\\5"
			  ),

		// No colons allowed in windows paths, other than in the root
		TEST("d:\\dd:\\dd:\\asdasd:\\",
			  "", "d:\\dd:\\dd:\\asdasd:\\",
			  "", ""),

		// No dots and spaces allowed in windows paths at the end of the path elements
		TEST("d:\\dd\\dd \\asdasd.\\",
			 "", "d:\\dd\\dd \\asdasd.\\",
			 "", "")
	};

	auto relative_windows_tests = std::vector<test> {
		TEST("/..//./1/2/../4///", // Path to normalize
			 "", "/1/4",           // Expected result on unix as any_kind (the relative_kind is expected to be invalid)
			 "\\1\\4", "\\1\\4"    // Expected result on windows, also as any_kind
			 )
	};


	/*****/

	for (auto &t: absolute_unix_tests) {
		test_normalize<Char, unix_format, absolute_kind>(t.line, t.path, t.expected_unix);
		test_normalize<Char, unix_format, any_kind>(t.line, t.path, t.expected_unix_any_kind);
		test_normalize<Char, windows_format, absolute_kind>(t.line, t.path, t.expected_windows);
		test_normalize<Char, windows_format, any_kind>(t.line, t.path, t.expected_windows_any_kind);
	}

	for (auto &t: relative_unix_tests) {
		test_normalize<Char, unix_format, relative_kind>(t.line, t.path, t.expected_unix);
		test_normalize<Char, unix_format, any_kind>(t.line, t.path, t.expected_unix_any_kind);
		test_normalize<Char, windows_format, relative_kind>(t.line, t.path, t.expected_windows);
		test_normalize<Char, windows_format, any_kind>(t.line, t.path, t.expected_windows_any_kind);
	}

	for (auto &t: absolute_windows_tests) {
		test_normalize<Char, unix_format, absolute_kind>(t.line, t.path, t.expected_unix);
		test_normalize<Char, unix_format, any_kind>(t.line, t.path, t.expected_unix_any_kind);
		test_normalize<Char, windows_format, absolute_kind>(t.line, t.path, t.expected_windows);
		test_normalize<Char, windows_format, any_kind>(t.line, t.path, t.expected_windows_any_kind);
	}

	for (auto &t: relative_windows_tests) {
		test_normalize<Char, unix_format, relative_kind>(t.line, t.path, t.expected_unix);
		test_normalize<Char, unix_format, any_kind>(t.line, t.path, t.expected_unix_any_kind);
		test_normalize<Char, windows_format, relative_kind>(t.line, t.path, t.expected_windows);
		test_normalize<Char, windows_format, any_kind>(t.line, t.path, t.expected_windows_any_kind);
	}
}

template <typename String>
void basic_path_test::test_append()
{
	using Char = typename String::value_type;

	{
		basic_path<Char, unix_format> b = S("/this/is/");
		auto p = b / S("a/path/");

		String expected = S("/this/is/a/path");

		CPPUNIT_ASSERT_EQUAL(expected, p.str());
	}

	{
		basic_path_list<Char, unix_format> b = { S("/this/is/"), S("/that/is/") };
		CPPUNIT_ASSERT_EQUAL(std::size_t(2), b.size());

		auto l = b / S("a/path/");

		{
			String expected1 = S("/this/is/a/path");
			String expected2 = S("/that/is/a/path");

			CPPUNIT_ASSERT_EQUAL(expected1, l[0].str());
			CPPUNIT_ASSERT_EQUAL(expected2, l[1].str());
		}

		l += S("/");

		CPPUNIT_ASSERT_EQUAL(std::size_t(3), l.size());

		l /= S("and this is another/path/");

		{
			String expected1 = S("/this/is/a/path/and this is another/path");
			String expected2 = S("/that/is/a/path/and this is another/path");
			String expected3 = S("/and this is another/path");

			CPPUNIT_ASSERT_EQUAL(expected1, l[0].str());
			CPPUNIT_ASSERT_EQUAL(expected2, l[1].str());
			CPPUNIT_ASSERT_EQUAL(expected3, l[2].str());
		}
	}

	{
		basic_path<Char, windows_format> b = S("C:\\root");
		auto p = b / S("this:is:illegal");

		String expected = {};

		CPPUNIT_ASSERT_EQUAL(expected, p.str());
	}
}

template <typename String>
void basic_path_test::test_base()
{
	using Char = typename String::value_type;

	String expected_without_suffixes = S("base");
	String expected_with_suffixes = S("base.with.suffixes");

	{
		basic_path<Char, unix_format> b = S("/this/is/a/base.with.suffixes");

		CPPUNIT_ASSERT_EQUAL(expected_without_suffixes, b.base(true).str());
		CPPUNIT_ASSERT_EQUAL(expected_with_suffixes, b.base(false).str());
	}

	{
		basic_path<Char, windows_format> b = S("X:\\this\\is\\a\\base.with.suffixes");

		CPPUNIT_ASSERT_EQUAL(expected_without_suffixes, b.base(true).str());
		CPPUNIT_ASSERT_EQUAL(expected_with_suffixes, b.base(false).str());
	}
}
