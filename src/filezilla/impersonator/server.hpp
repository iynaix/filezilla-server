#ifndef FZ_IMPERSONATOR_SERVER_HPP
#define FZ_IMPERSONATOR_SERVER_HPP

#include "../logger/modularized.hpp"

#include "channel.hpp"
#include "../tvfs/backends/local_filesys.hpp"

namespace fz::impersonator {

class server
{
public:
	server(fz::logger_interface &logger, int in_fd, int out_fd);

	int run();

private:
	void open_file(const fz::native_string &path, fz::file::mode mode, fz::file::creation_flags flags);
	void open_directory(const fz::native_string &path);
	void rename(const fz::native_string &from, const fz::native_string &to);
	void remove_file(const fz::native_string &path);
	void remove_directory(const fz::native_string &path, bool recursive);
	void info(const fz::native_string &path, bool follow_links);
	void mkdir(const fz::native_string &path, bool recurse, mkdir_permissions mkdir_permissions);
	void set_mtime(const fz::native_string &path, const fz::datetime &mtime);

private:
	event_loop event_loop_;
	fz::logger::modularized logger_;
	fz::impersonator::channel channel_;
	tvfs::backends::local_filesys backend_;

	int error_ = 0;
};

}
#endif // FZ_IMPERSONTOR_SERVER_HPP
