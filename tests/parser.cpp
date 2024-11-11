#include "test_utils.hpp"

#include "../src/filezilla/util/parser.hpp"
#include "../src/filezilla/hostaddress.hpp"

/*
 * This testsuite asserts the correctness of the parser class of functions.
 */

class parser_test final : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(parser_test);
	CPPUNIT_TEST(test_ipv6);
	CPPUNIT_TEST(test_eprt);
	CPPUNIT_TEST_SUITE_END();

public:
	~parser_test() override
	{
		delete [] runtime_string;
	}

	void test_ipv6();
	void test_eprt();

	std::string_view runtime(std::string_view str)
	{
		delete [] runtime_string;
		runtime_string = new volatile char[str.size()+1]{};
		std::copy(str.begin(), str.end(), runtime_string);
		return const_cast<const char*>(runtime_string);
	}

private:
	volatile char *runtime_string{};
};

CPPUNIT_TEST_SUITE_REGISTRATION(parser_test);

#define for_indexed(...) for_indexed_v(i, __VA_ARGS__)
#define for_indexed_v(v, ...) if (std::size_t v = std::size_t(-1); false); else for (__VA_ARGS__) if ((++v, false)); else

void parser_test::test_ipv6()
{
	using namespace fz;

	auto ipv6_test = [](std::string_view ip) constexpr
	{
		hostaddress::ipv6_host ipv6{9, 8, 7, 6, 5, 4, 3, 2};

		util::parseable_range r(ip);

		auto succeeded = parse_ip(r, ipv6) && eol(r);
		return std::pair(succeeded, ipv6);
	};

	{
		constexpr std::string_view string = "::1";

		constexpr auto constexpr_result = ipv6_test(string);
		auto runtime_result = ipv6_test(runtime(string));

		for_indexed (auto [parsing_succeeded, ipv6]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, parsing_succeeded);

			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[0]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[1]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[2]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[3]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[4]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[5]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0), ipv6[6]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(1), ipv6[7]);
		}
	}

	{
		constexpr std::string_view string = "123:456:789::8765:4321";
		constexpr auto constexpr_result = ipv6_test(string);
		auto runtime_result = ipv6_test(runtime(string));

		for_indexed (auto [parsing_succeeded, ipv6]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, parsing_succeeded);

			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0123), ipv6[0]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0456), ipv6[1]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0789), ipv6[2]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0000), ipv6[3]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0000), ipv6[4]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0000), ipv6[5]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x8765), ipv6[6]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x4321), ipv6[7]);
		}
	}


	{
		constexpr std::string_view string = "1:2:3:4:5:6:7:8.9.10.42";
		constexpr auto constexpr_result = ipv6_test(string);
		auto runtime_result = ipv6_test(runtime(string));

		for_indexed (auto [parsing_succeeded, _]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, !parsing_succeeded);
		}
	}
}

void parser_test::test_eprt()
{
	using namespace fz;

	auto eprt_test = [](std::string_view str) constexpr
	{
		util::parseable_range r(str);

		address_type ipv{};
		hostaddress::ipv4_host ipv4{127, 125, 124, 123};
		hostaddress::ipv6_host ipv6{9, 8, 7, 6, 5, 4, 3, 2};

		std::uint16_t port{};

		auto succeeded = parse_eprt_cmd(r, ipv, ipv4, ipv6, port);
		return std::pair(succeeded, std::tuple(ipv, ipv4, ipv6, port));
	};

	{
		constexpr std::string_view string = "|2|1:2:3:4:5:6:7:8.9.10.42|1234|";
		constexpr auto constexpr_result = eprt_test(string);
		auto runtime_result = eprt_test(runtime(string));

		for_indexed (auto [parsing_succeeded, _]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, !parsing_succeeded);
		}
	}

	{
		constexpr std::string_view string = "|2|1::3:4:5:6:7:8|1234|";
		constexpr auto constexpr_result = eprt_test(string);
		auto runtime_result = eprt_test(runtime(string));

		for_indexed (auto [parsing_succeeded, data]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, parsing_succeeded);

			auto [ipv, ipv4, ipv6, port] = data;

			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, address_type::ipv6, ipv);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(1234), port);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0001), ipv6[0]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0000), ipv6[1]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0003), ipv6[2]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0004), ipv6[3]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0005), ipv6[4]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0006), ipv6[5]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0007), ipv6[6]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(0x0008), ipv6[7]);
		}
	}

	{
		constexpr std::string_view string = "|1|127.126.125.124|1234|";
		constexpr auto constexpr_result = eprt_test(string);
		auto runtime_result = eprt_test(runtime(string));

		for_indexed (auto [parsing_succeeded, data]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, parsing_succeeded);

			auto [ipv, ipv4, ipv6, port] = data;

			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, address_type::ipv4, ipv);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint16_t(1234), port);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint8_t(127), ipv4[0]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint8_t(126), ipv4[1]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint8_t(125), ipv4[2]);
			CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, std::uint8_t(124), ipv4[3]);
		}
	}

	{
		constexpr std::string_view string = "|1|322.126.125.124|1234|";
		constexpr auto constexpr_result = eprt_test(string);
		auto runtime_result = eprt_test(runtime(string));

		for_indexed (auto [parsing_succeeded, data]: { constexpr_result, runtime_result }) {
			std::string msg = i == 0 ? "Type    : constexpr" : "Type    : runtime";

			CPPUNIT_ASSERT_MESSAGE(msg, !parsing_succeeded);
		}
	}
}
