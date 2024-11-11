#ifndef FZ_IMPERSONATOR_PARENT_PROXY_HPP
#define FZ_IMPERSONATOR_PARENT_PROXY_HPP

#include "../logger/modularized.hpp"

#include "channel.hpp"

#if defined(FZ_WINDOWS) && FZ_WINDOWS
#	include <stdio.h>
#endif

namespace fz::impersonator {

class parent_proxy: public fdreadwriter
{
public:
	parent_proxy(logger_interface &logger, int in_fd, int out_fd);
	~parent_proxy() override;

	int send_fd(buffer &buf, fd_owner &fd) override;
	int read_fd(buffer &buf, fd_owner &fd) override;

	void set_event_handler(event_handler *eh) override;

private:
	logger::modularized logger_;
	event_handler *eh_{};

#if defined(FZ_WINDOWS) && FZ_WINDOWS
	fz::file in_{};
	fz::file out_{};
	HANDLE parent_;
#else
	int in_;
	int out_;
#endif
};

}
#endif // PARENT_PROXY_HPP
