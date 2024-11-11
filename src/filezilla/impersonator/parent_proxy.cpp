#include "parent_proxy.hpp"

#if !defined(FZ_WINDOWS)

#include <libfilezilla/glue/unix.hpp>

fz::impersonator::parent_proxy::parent_proxy(logger_interface &logger, int in_fd, int out_fd)
	: fdreadwriter(this)
	, logger_(logger)
	, in_(in_fd)
	, out_(out_fd)
{
}

fz::impersonator::parent_proxy::~parent_proxy()
{}

int fz::impersonator::parent_proxy::send_fd(buffer &buf, fd_owner &fd)
{
	int error = 0;

	fz::send_fd(out_, buf, fd.get(), error);

	if (error != EAGAIN)
		fd.reset();

	return error;
}

int fz::impersonator::parent_proxy::read_fd(buffer &buf, fd_owner &fd)
{
	fd_t in_fd = invalid_fd;
	int error = 0;
	int res = fz::read_fd(in_, buf, in_fd, error);
	fd = fd_owner(in_fd);

	if (res == 0)
		error = ENODATA;

	return error;
}

#else

#include <io.h>
#include "../util/io.hpp"

fz::impersonator::parent_proxy::parent_proxy(fz::logger_interface &logger, int in_fd, int out_fd)
	: fdreadwriter(this)
	, logger_(logger, "impersonator parent_proxy")
	, in_(fd_t(_get_osfhandle(in_fd)))
	, out_(fd_t(_get_osfhandle(out_fd)))
{
	if (!in_ || !out_) {
		logger_.log_u(logmsg::error, L"Invalid file descriptor(s).");
		return;
	}
}

fz::impersonator::parent_proxy::~parent_proxy()
{
	in_.detach();
	out_.detach();
}

int fz::impersonator::parent_proxy::send_fd(buffer &buf, fd_owner &fd)
{
	int error;

	if (!util::io::write(out_, fd.release(), &error))
		return error;

	if (!util::io::write(out_, buf.size(), &error))
		return error;

	if (!util::io::write(out_, buf, &error))
		return error;

	buf.clear();
	return 0;
}

int fz::impersonator::parent_proxy::read_fd(buffer &buf, fd_owner &fd)
{
	int error;
	fd_t in_fd;

	if (!util::io::read(in_, in_fd, &error))
		return error;

	fd = fd_owner(in_fd);

	std::size_t size = 0;
	if (!util::io::read(in_, size, &error))
		return error;

	if (size > input_archive::max_payload_size) {
		return EFBIG;
	}

	if (!util::io::read(in_, buf.get(size), size, &error))
		return error;

	buf.add(size);
	return 0;
}

#endif

void fz::impersonator::parent_proxy::set_event_handler(event_handler *eh)
{
	eh_ = eh;
}
