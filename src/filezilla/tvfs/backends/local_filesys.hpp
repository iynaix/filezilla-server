#ifndef FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP
#define FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP

#include "../../logger/modularized.hpp"
#include "../backend.hpp"

namespace fz::tvfs::backends {

class local_filesys final: public backend
{
public:
	local_filesys(logger_interface &logger);

	void open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r) override;
	void open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r) override;
	void rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r) override;
	void remove_file(const absolute_native_path &path, receiver_handle<remove_response> r) override;
	void remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r) override;
	void info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r) override;
	void mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r) override;
	void set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r) override;

private:
	logger::modularized logger_;
};

}
#endif // FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP
