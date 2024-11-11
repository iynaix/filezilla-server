#include <libfilezilla/util.hpp>
#include <libfilezilla/encode.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/recursive_remove.hpp>

#ifdef FZ_WINDOWS
#	include <fileapi.h>
#else
#	include <unistd.h>
#endif

#include "../src/filezilla/logger/null.hpp"
#include "../src/filezilla/tvfs/engine.hpp"

#include "test_utils.hpp"

/*
 * This testsuite asserts the correctness of the tvfs class.
 */

class tvfs_test final : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(tvfs_test);
	CPPUNIT_TEST(test_read_only_root);
	CPPUNIT_TEST(test_read_write_root);
	CPPUNIT_TEST(test_cwd);
	CPPUNIT_TEST(test_non_recursive_root);
	CPPUNIT_TEST(test_holes);
	CPPUNIT_TEST(test_remove);
	CPPUNIT_TEST(test_rename);
	CPPUNIT_TEST(test_set_mtime);
	CPPUNIT_TEST(test_limits);
	CPPUNIT_TEST_SUITE_END();

public:
	tvfs_test();
	~tvfs_test() override;

	void setUp() override;
	void tearDown() override;

	void test_read_only_root();
	void test_read_write_root();
	void test_cwd();
	void test_non_recursive_root();
	void test_holes();
	void test_remove();
	void test_rename();
	void test_set_mtime();
	void test_limits();

private:
	fz::native_string get_tests_rootdir();
	void set_mount_table(fz::tvfs::mount_table &&table);

	fz::tvfs::engine tvfs_;
	fz::util::fs::native_path native_root_;
	fz::util::fs::unix_path tvfs_root_;
};

CPPUNIT_TEST_SUITE_REGISTRATION(tvfs_test);

#define test_file(i) "test_file_" #i
#define native_test_file(i) fzT("test_file_" #i)

#define test_dir(i) "test_dir_" #i
#define native_test_dir(i) fzT("test_dir_" #i)

tvfs_test::tvfs_test()
	: tvfs_(fz::logger::null)
	, tvfs_root_("/")
{
}

tvfs_test::~tvfs_test()
{}

void tvfs_test::setUp() {

	int max_num_attempts = 5;
	int i = 0;
	do {
		auto root_name = fzT("tvfs_test") + fz::to_native(fz::base32_encode(fz::random_bytes(10), fz::base32_type::locale_safe, false));

		native_root_ = get_tests_rootdir();
		native_root_ /= root_name;

		if (fz::mkdir(native_root_, true))
			break;
	} while (++i != max_num_attempts);

	CPPUNIT_ASSERT_MESSAGE("Couldn't create tvfs native root directory: maximum number of attempts reached", i != max_num_attempts);
}

void tvfs_test::tearDown() {
	fz::recursive_remove r;
	r.remove(native_root_);
}

void tvfs_test::test_read_only_root()
{
	{
		auto f = (native_root_ / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
		CPPUNIT_ASSERT(f.opened());
	}

	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_only, fz::tvfs::mount_point::apply_permissions_recursively }
	});

	fz::tvfs::file_holder f;

	auto res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::writing, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::readwrite, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::reading, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
}

void tvfs_test::test_cwd()
{
	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively },
		{ "/dummy", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively }
	});

	fz::tvfs::file_holder f;

	auto res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::writing, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	auto prev_dir = tvfs_.get_current_directory();

	res = tvfs_.set_current_directory(tvfs_root_ / test_dir(non_existing_entry));
	CPPUNIT_ASSERT_EQUAL(fz::result::other, res.error_);
	CPPUNIT_ASSERT_EQUAL(prev_dir.str(), tvfs_.get_current_directory().str());

	res = tvfs_.set_current_directory(tvfs_root_ / test_file(0));
	CPPUNIT_ASSERT_EQUAL(fz::result::nodir, res.error_);
	CPPUNIT_ASSERT_EQUAL(prev_dir.str(), tvfs_.get_current_directory().str());

	res = tvfs_.set_current_directory(tvfs_root_ / "dummy");
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT_EQUAL((tvfs_root_ / "dummy").str(), tvfs_.get_current_directory().str());
}

void tvfs_test::test_read_write_root()
{
	{
		auto f = (native_root_ / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
		CPPUNIT_ASSERT(f.opened());
	}

	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::do_not_apply_permissions_recursively }
	});

	fz::tvfs::file_holder f;

	auto res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::writing, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::reading, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = tvfs_.open_file(f, tvfs_root_ / test_file(0), fz::file::readwrite, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
}

void tvfs_test::test_non_recursive_root()
{
	fz::result res;
	fz::tvfs::file_holder f;
	fz::tvfs::entries_iterator it;

	res = fz::mkdir(native_root_ / native_test_dir(0), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = fz::mkdir(native_root_ / fzT("foo") / native_test_dir(0), true);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	fz::file f2 = (native_root_ / native_test_dir(0) / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
	CPPUNIT_ASSERT(f2.opened());
	f = {};

	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::do_not_apply_permissions_recursively },
		{ "/foo/bar", native_root_ / native_test_dir(0), fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::do_not_apply_permissions_recursively }
	});

	res = tvfs_.open_file(f, tvfs_root_ / test_dir(0) / test_file(0), fz::file::writing, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	res = tvfs_.open_file(f, tvfs_root_ / test_dir(0) / test_file(0), fz::file::reading, 0);
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	res = tvfs_.set_current_directory(tvfs_root_ / test_dir(0));
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	// Checking whether the listing of / matches with the expected one
	res = tvfs_.get_entries(it, tvfs_root_, fz::tvfs::traversal_mode::only_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	CPPUNIT_ASSERT(it.has_next());
	CPPUNIT_ASSERT_EQUAL(std::string(test_dir(0)), it.next().name());

	CPPUNIT_ASSERT(it.has_next());
	CPPUNIT_ASSERT_EQUAL(std::string("foo"), it.next().name());

	CPPUNIT_ASSERT(!it.has_next());

	// It shouldn't be able to list test_dir(0)
	res = tvfs_.get_entries(it, tvfs_root_ / test_dir(0), fz::tvfs::traversal_mode::only_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);
	CPPUNIT_ASSERT(!it.has_next());

	// It should be able to list "/foo" nonetheless
	res = tvfs_.get_entries(it, "/foo", fz::tvfs::traversal_mode::only_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	//But it shouldn't be able to list its real content, only its sole mountpoint.
	CPPUNIT_ASSERT_EQUAL(std::string("bar"), it.next().name());
	CPPUNIT_ASSERT(!it.has_next());

	// And it should be able to list "/foo/bar"
	res = tvfs_.get_entries(it, "/foo/bar", fz::tvfs::traversal_mode::only_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());
	CPPUNIT_ASSERT_EQUAL(std::string(test_file(0)), it.next().name());

	CPPUNIT_ASSERT(!it.has_next());
}

void tvfs_test::test_holes()
{
	fz::result res;
	fz::file f;
	fz::tvfs::entries_iterator it;

	f = (native_root_ / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
	CPPUNIT_ASSERT(f.opened());
	f.close();

	set_mount_table({
		{ "/foo/bar", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification }
	});

	// No permissions to create directories under an "implicit" mountpoint
	res = tvfs_.make_directory(tvfs_root_ / "foo" / test_file(0)).first;
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	// It should be able to list "/"
	res = tvfs_.get_entries(it, tvfs_root_, fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	CPPUNIT_ASSERT_EQUAL(std::string("foo"), it.next().name());
	CPPUNIT_ASSERT(!it.has_next());

	// It should be able to list "/foo"
	res = tvfs_.get_entries(it, tvfs_root_ / "foo", fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	CPPUNIT_ASSERT_EQUAL(std::string("bar"), it.next().name());
	CPPUNIT_ASSERT(!it.has_next());
}

void tvfs_test::test_remove()
{
	fz::result res;
	fz::file f;
	fz::tvfs::entries_iterator it;

	res = fz::mkdir(native_root_ / native_test_dir(0), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	f = (native_root_ / native_test_dir(0) / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
	CPPUNIT_ASSERT(f.opened());
	f.close();

	set_mount_table({
		{ "/dir0", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification },
	});

	res = tvfs_.get_entries(it, tvfs_root_ / "dir0", fz::tvfs::traversal_mode::no_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	// You can't remove a mountpoint
	res = tvfs_.remove_entry(it.next());
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	// And you can't remove a non empty directory
	res = tvfs_.get_entries(it, tvfs_root_ / "dir0" / test_dir(0), fz::tvfs::traversal_mode::no_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	res = tvfs_.remove_entry(it.next());
	CPPUNIT_ASSERT_EQUAL(fz::result::other, res.error_);

	// But you can remove the file under that directory
	res = tvfs_.get_entries(it, tvfs_root_ / "dir0" / test_dir(0), fz::tvfs::traversal_mode::only_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	auto file_entry = it.next();
	CPPUNIT_ASSERT_EQUAL(std::string(test_file(0)), file_entry.name());

	res = tvfs_.remove_entry(file_entry);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	// And then you can remove the directory that contained it
	res = tvfs_.get_entries(it, tvfs_root_ / "dir0" / test_dir(0), fz::tvfs::traversal_mode::no_children);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(it.has_next());

	res = tvfs_.remove_entry(it.next());
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
}

void tvfs_test::test_rename()
{
	fz::result res;
	fz::file f;
	fz::tvfs::entries_iterator it;

	res = fz::mkdir(native_root_ / native_test_dir(0), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = fz::mkdir(native_root_ / native_test_dir(1), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = fz::mkdir(native_root_ / native_test_dir(0) / native_test_dir(0), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = fz::mkdir(native_root_ / native_test_dir(1) / native_test_file(0), false);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	set_mount_table({
		{ "/dir0", native_root_ / native_test_dir(0), fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification },
		{ "/dir1", native_root_ / native_test_dir(1), fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification }
	});

	// You can't rename a mountpoint
	res = tvfs_.rename("/dir0", "/dir1/dir0");
	CPPUNIT_ASSERT_EQUAL(fz::result::noperm, res.error_);

	// But you can rename a normal directory
	res = tvfs_.rename(tvfs_root_ / "dir0" / test_dir(0), tvfs_root_ / "dir1" / test_dir(0));
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	// It should be able to list the just renamed directory, which shouldn't be found anymore at the original place
	res = tvfs_.get_entries(it, tvfs_root_ / "dir1" / test_dir(0), fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = tvfs_.get_entries(it, tvfs_root_ / "dir0" / test_dir(0), fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::other, res.error_);

	// And you can rename a normal file
	res = tvfs_.rename(tvfs_root_ / "dir1" / test_file(0), tvfs_root_ / "dir0" / test_file(1));
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	// It should be able to stat the just renamed file, which shouldn't be found anymore at the original place
	res = tvfs_.get_entries(it, tvfs_root_ / "dir0" / test_file(1), fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	res = tvfs_.get_entries(it, tvfs_root_ / "dir1" / test_file(0), fz::tvfs::traversal_mode::autodetect);
	CPPUNIT_ASSERT_EQUAL(fz::result::other, res.error_);
}

void tvfs_test::test_set_mtime()
{
	auto f = (native_root_ / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
	CPPUNIT_ASSERT(f.opened());
	f.close();

	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_write, fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification },
	});

	auto [res, entry] = tvfs_.get_entry(tvfs_root_ / test_file(0));
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);

	auto old_mtime = entry.mtime();
	auto test_mtime = fz::datetime(fz::datetime::utc, 2037, 5, 4, 3, 2, 1, 0);

	std::tie(res, entry) = tvfs_.set_mtime(tvfs_root_ / test_file(0), test_mtime);
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT(entry.mtime() != old_mtime);
	CPPUNIT_ASSERT_EQUAL(test_mtime.get_rfc822(), entry.mtime().get_rfc822());

	std::tie(res, entry) = tvfs_.get_entry(tvfs_root_ / test_file(0));
	CPPUNIT_ASSERT_EQUAL(fz::result::ok, res.error_);
	CPPUNIT_ASSERT_EQUAL(test_mtime.get_rfc822(), entry.mtime().get_rfc822());
}

void tvfs_test::test_limits()
{
	{
		auto f = (native_root_ / native_test_file(0)).open(fz::file::writing, fz::file::creation_flags::empty);
		CPPUNIT_ASSERT(f.opened());
	}

	auto limit_files = fz::tvfs::open_limits::type(fz::random_number(2, 10));
	auto limit_dirs = fz::tvfs::open_limits::type(fz::random_number(2, 10));

	tvfs_.set_open_limits({limit_files, limit_dirs});

	set_mount_table({
		{ "/", native_root_, fz::tvfs::mount_point::read_only },
	});

	std::vector<fz::tvfs::file_holder> files(limit_files+1);
	std::vector<fz::tvfs::entries_iterator> dirs(limit_dirs+1);

	for (std::size_t i = 0; i < files.size(); ++i) {
		auto res = tvfs_.open_file(files[i], tvfs_root_ / test_file(0), fz::file::reading, 0);
		CPPUNIT_ASSERT_EQUAL_MESSAGE(fz::sprintf("Open files limit is %d. Current file idx is %d", limit_files, i), i == limit_files ? fz::result::noperm : fz::result::ok, res.error_);
	}

	for (std::size_t i = 0; i < dirs.size(); ++i) {
		auto res = tvfs_.get_entries(dirs[i], tvfs_root_, fz::tvfs::traversal_mode::only_children);
		CPPUNIT_ASSERT_EQUAL_MESSAGE(fz::sprintf("Open dirs limit is %d. Current dir idx is %d", limit_dirs, i), i == limit_dirs ? fz::result::noperm : fz::result::ok, res.error_);
	}

	tvfs_.set_open_limits({});
}

fz::native_string tvfs_test::get_tests_rootdir()
{
	fz::native_string tests_root_dir;

#ifdef FZ_WINDOWS
	auto size = GetCurrentDirectoryW(0, nullptr);
	CPPUNIT_ASSERT_MESSAGE("GetCurrentDirectoryW failed", size != 0);

	tests_root_dir.resize(std::size_t(size-1));
	size = GetCurrentDirectoryW(size, tests_root_dir.data());
	CPPUNIT_ASSERT_MESSAGE("GetCurrentDirectoryW failed", size != 0);
#else
	const char *cwd = nullptr;

	tests_root_dir.resize(64);
	do {
		tests_root_dir.resize(tests_root_dir.size()*2);
		cwd = getcwd(tests_root_dir.data(), tests_root_dir.size()+1);
	} while (!cwd && errno == ERANGE);

	CPPUNIT_ASSERT_MESSAGE("Couldn't get cwd", cwd != nullptr);

	tests_root_dir.resize(std::char_traits<fz::native_string::value_type>::length(tests_root_dir.data()));
#endif

	CPPUNIT_ASSERT(!tests_root_dir.empty());

	return tests_root_dir;
}

void tvfs_test::set_mount_table(fz::tvfs::mount_table &&table)
{
	tvfs_.set_mount_tree(std::make_shared<fz::tvfs::mount_tree>(std::move(table)));
}
