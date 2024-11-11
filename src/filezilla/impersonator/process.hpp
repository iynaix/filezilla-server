#ifndef FZ_IMPERSONATOR_PROCESS_HPP
#define FZ_IMPERSONATOR_PROCESS_HPP

#include <libfilezilla/process.hpp>

#include "channel.hpp"

namespace fz::impersonator {

class process: private event_handler, public fdreadwriter
{
public:
	process(event_loop &event_loop, thread_pool &thread_pool, logger_interface &logger, const native_string &exe, const impersonation_token &token);
	~process() override;

	int send_fd(buffer &buf, fd_owner &fd) override;
	int read_fd(buffer &buf, fd_owner &fd) override;

	void set_event_handler(event_handler *eh) override;

private:
	void operator()(const event_base &ev) override;

	mutex mutex_;
	thread_pool &thread_pool_;
	logger::modularized logger_;
	fz::process fzp_;
	event_handler *eh_{};

	#if !(defined(FZ_WINDOWS) && FZ_WINDOWS)
		std::unique_ptr<socket> sock_;
	#else
		int write(fz::process &p, std::initializer_list<std::pair<const void *, std::size_t>> data);
		int write(fz::process &p, buffer &buf);

		fz::buffer out_buf_;
		bool waiting_for_write_event_{};
		bool must_send_write_event_{};

		struct header
		{
			fd_t fd;
			std::size_t payload_size;

			header()
				: header(invalid_fd, 0)
			{
			}

			header(fd_t fd, std::size_t payload_size)
			{
				std::memset(this, 0, sizeof(*this));

				this->fd = fd;
				this->payload_size = payload_size;
			}
		};

		enum state
		{
			processing_header,
			processing_payload
		};

		header in_header_{};
		state in_state_{processing_header};
		std::size_t header_size_read_{};
		bool must_send_read_event_{true};

		void on_process_read();
		void on_process_write();
	#endif
};

}
#endif // FZ_IMPERSONATOR_PROCESS_HPP
