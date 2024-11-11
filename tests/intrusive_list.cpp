#include <libfilezilla/util.hpp>
#include <libfilezilla/encode.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/recursive_remove.hpp>

#include "test_utils.hpp"

#include "../src/filezilla/intrusive_list.hpp"

/*
 * This testsuite asserts the correctness of the intrusive_list class.
 */

class intrusive_list_test final : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(intrusive_list_test);
	CPPUNIT_TEST(test_insert_and_clear);
	CPPUNIT_TEST(test_insert_and_remove);
	CPPUNIT_TEST_SUITE_END();

public:
	void test_insert_and_clear();
	void test_insert_and_remove();
};

CPPUNIT_TEST_SUITE_REGISTRATION(intrusive_list_test);

struct test_node: fz::intrusive_node
{
	test_node(int i)
		: i(i)
	{
		total_sum += i;
	}

	int i;

	static int total_sum;
};

int test_node::total_sum = 0;

void intrusive_list_test::test_insert_and_clear()
{
	using namespace fz;

	intrusive_list<test_node> list;

	test_node::total_sum = 0;

	test_node node1(1);
	test_node node2(2);
	test_node node3(3);
	test_node node4(4);
	test_node node5(5);

	list.push_back(node5);
	list.push_back(node3);
	list.push_back(node2);
	list.push_back(node1);
	list.push_back(node4);

	int sum = 0;

	for (auto &n: list) {
		sum += n.i;
	}

	CPPUNIT_ASSERT_EQUAL(sum, test_node::total_sum);

	list.clear();

	CPPUNIT_ASSERT(list.empty());
}

void intrusive_list_test::test_insert_and_remove()
{
	using namespace fz;

	intrusive_list<test_node> list;

	test_node::total_sum = 0;

	test_node node1(1);
	test_node node2(2);
	test_node node3(3);
	test_node node4(4);
	test_node node5(5);

	list.push_back(node5);
	list.push_back(node3);
	list.push_back(node2);
	list.push_back(node1);
	list.push_back(node4);

	int sum = 0;

	for (auto &n: list) {
		sum += n.i;
	}

	CPPUNIT_ASSERT_EQUAL(sum, test_node::total_sum);

	while (!list.empty()) {
		auto &n = list.back();
		sum -= n.i;
		n.remove();
	}

	CPPUNIT_ASSERT_EQUAL(0, sum);
}
