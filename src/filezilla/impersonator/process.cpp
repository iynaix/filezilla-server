#include <libfilezilla/impersonation.hpp>

#include "process.hpp"
#include "../remove_event.hpp"

#include <cassert>

fz::impersonator::process::~process()
{
	logger_.log_u(logmsg::debug_debug, L"EXITING!");
	remove_handler();
	remove_events<socket_event>(eh_, this);
}

#if !defined(FZ_WINDOWS)

#include <libfilezilla/glue/unix.hpp>

fz::impersonator::process::process(event_loop &event_loop, thread_pool &thread_pool, logger_interface &logger, const native_string &exe, const impersonation_token &token)
	: event_handler(event_loop)
	, fdreadwriter(this)
	, thread_pool_(thread_pool)
	, logger_(logger, "impersonator process")
{
	if (!token) {
		logger_.log_u(logmsg::error, L"Token is invalid");
		return;
	}

	logger_.log_u(logmsg::debug_info, L"Token is valid");

	fd_owner parent_fd;
	fd_owner child_fd;

	{
		int fds[2];

		if (!create_socketpair(fds)) {
			logger_.log_u(logmsg::error, L"Could not create socket pair");
			return;
		}

		parent_fd = fd_owner(fds[0]);
		child_fd = fd_owner(fds[1]);
	}

	logger_.log_u(logmsg::debug_info, L"Socket pair created.");

	int err = 0;
	sock_ = socket::from_descriptor(socket_descriptor(parent_fd.release()), thread_pool_, err);
	if (!sock_) {
		logger_.log_u(logmsg::error, L"Could not create fz::socket. Reason: %s", socket_error_description(err));
		return;
	}

	if (!fzp_.spawn(token, exe, { "MAGIC_VALUE!", fz::to_string(child_fd.get()), fz::to_string(child_fd.get())}, {child_fd.get()}, fz::process::io_redirection::closeall)) {
		logger_.log_u(logmsg::error, L"Could not spawn process \"%s\"", exe);
		return;
	}

	logger_.log_u(logmsg::debug_info, L"Process \"%s\" spawned. Ready to go.", exe);
}

void fz::impersonator::process::set_event_handler(event_handler *eh)
{
	scoped_lock lock(mutex_);

	if (eh_ == eh || !sock_)
		return;

	eh_ = eh;

	if (!eh_)
		sock_->set_event_handler(nullptr);
	else
		sock_->set_event_handler(this, socket_event_flag::write | socket_event_flag::read);
}

void fz::impersonator::process::operator()(const event_base &ev)
{
	fz::dispatch<socket_event>(ev, [&](socket_event_source *, socket_event_flag flag, int err) {
		scoped_lock lock(mutex_);

		logger_.log_u(logmsg::debug_debug, L"%s: flag: %d, err: %d, eh: %p", __PRETTY_FUNCTION__, flag, err, eh_);

		if (eh_)
			eh_->send_event<socket_event>(this, flag, err);
	});
}

int fz::impersonator::process::send_fd(buffer &buf, tvfs::fd_owner &fd)
{
	if (!sock_)
		return EBADF;

	int error = 0;

	sock_->send_fd(buf, fd.get(), error);

	if (error != EAGAIN)
		fd.reset();

	return error;
}

int fz::impersonator::process::read_fd(buffer &buf, fd_owner &fd)
{
	if (!sock_)
		return EBADF;

	fd_t in_fd = invalid_fd;
	int error = 0;

	int res = sock_->read_fd(buf, in_fd, error);
	fd = fd_owner(in_fd);

	if (res == 0) {
		logger_.log_raw(logmsg::error, L"process:read_fd: Unexpected EOF");
		error = ENODATA;
	}

	return error;
}

#else

int fz::impersonator::process::write(fz::process &p, std::initializer_list<std::pair<const void *, std::size_t>> data)
{
	if (waiting_for_write_event_) {
		must_send_write_event_ = true;
		return EAGAIN;
	}

	for (auto it = data.begin(); it != data.end(); ++it) {
		auto pv = reinterpret_cast<const char*>(it->first);
		auto size = it->second;

		while (size) {
			auto res = p.write(pv, size);

			if (res.error_ == res.wouldblock) {
				waiting_for_write_event_ = true;
				out_buf_.append(reinterpret_cast<const unsigned char*>(pv), size);

				while (++it != data.end()) {
					out_buf_.append(reinterpret_cast<const unsigned char*>(it->first), it->second);
				}

				return 0;
			}

			if (!res)
				return EIO;

			pv += res.value_;
			size -= res.value_;
		}
	}

	return 0;
}

fz::impersonator::process::process(event_loop &event_loop, thread_pool &thread_pool, logger_interface &logger, const native_string &exe, const impersonation_token &token)
	: event_handler(event_loop)
	, fdreadwriter(this)
	, thread_pool_(thread_pool)
	, logger_(logger, "impersonator process")
	, fzp_(thread_pool, *this)
{
	if (!token) {
		logger_.log_u(logmsg::error, L"Token is invalid");
		return;
	}

	logger_.log_u(logmsg::debug_info, L"Token is valid");

	if (!fzp_.spawn(token, exe, { fzT("MAGIC_VALUE!"), fzT("0"), fzT("1") }, fz::process::io_redirection::redirect)) {
		logger_.log_u(logmsg::error, L"Could not spawn process %s", exe);
		return;
	}

	logger_.log_u(logmsg::debug_info, L"Process %s spawned.", exe);
}


int fz::impersonator::process::send_fd(buffer &buf, fd_owner &fd)
{
	scoped_lock lock(mutex_);

	if (buf.size() == 0) {
		logger_.log_raw(logmsg::error, L"process::send_fd(): payload size is 0.");
		return EINVAL;
	}

	fd_t duplicated = invalid_fd;
	if (fd && !DuplicateHandle(GetCurrentProcess(), fd.get(), fzp_.handle(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
		logger_.log_u(logmsg::error, L"process::send_fd: could not duplicate handle. Windows error: 0x%X", GetLastError());
		return EIO;
	}

	int err;
	header h{duplicated, buf.size()};

	if (err = write(fzp_, {{&h, sizeof(h)}, {buf.get(), buf.size()}}); err)
		return err;

	fd.reset();
	buf.clear();

	return 0;
}

int fz::impersonator::process::read_fd(buffer &buf, fd_owner &fd)
{
	scoped_lock lock(mutex_);

	if (in_state_ == processing_payload) {
		fd = fd_owner();

		while (in_header_.payload_size > 0) {
			auto res = fzp_.read(buf.get(in_header_.payload_size), in_header_.payload_size);
			if (res.error_ == res.wouldblock)
				return EAGAIN;

			if (!res) {
				logger_.log_u(logmsg::error, L"process::read_fd: process::read() returned error: %d, raw: %d", res.error_, res.raw_);
				return EIO;
			}

			if (!res.value_) {
				logger_.log_raw(logmsg::error, L"process::read_fd: got premature EOF");
				return ENODATA;
			}

			if (is_valid_fd(in_header_.fd)) {
				fd = fd_owner(in_header_.fd);
				in_header_.fd = invalid_fd;
			}

			in_header_.payload_size -= res.value_;
			buf.add(res.value_);
		}

		in_state_ = processing_header;
		send_event<process_event>(&fzp_, process_event_flag::read);
		header_size_read_ = 0;
		return 0;
	}
	else {
		must_send_read_event_ = true;
		return EAGAIN;
	}
}

void fz::impersonator::process::on_process_read()
{
	if (in_state_ == processing_payload)
		return eh_->send_event<socket_event>(this, socket_event_flag::read, 0);

	auto p = reinterpret_cast<char *>(&in_header_) + header_size_read_;
	auto header_remaining_size = sizeof(header) - header_size_read_;

	while (header_remaining_size) {
		auto res = fzp_.read(p, header_remaining_size);
		if (res.error_ == res.wouldblock)
			return;

		if (!res) {
			logger_.log_u(logmsg::error, L"process::on_process_read: process::read() returned error: %d, raw: %d", res.error_, res.raw_);
			return eh_->send_event<socket_event>(this, socket_event_flag::read, EIO);
		}

		if (!res.value_) {
			logger_.log_raw(logmsg::error, L"process::on_process_read: got premature EOF");
			return eh_->send_event<socket_event>(this, socket_event_flag::read, ENODATA);
		}

		header_remaining_size -= res.value_;
		header_size_read_ += res.value_;
		p += res.value_;
	}

	logger_.log_u(logmsg::debug_debug, L"process::on_process_read: got header: fd = %d, payload_size = %d", std::intptr_t(in_header_.fd), in_header_.payload_size);

	if (in_header_.payload_size == 0) {
		logger_.log_raw(logmsg::error, L"process::on_process_read: got 0 as payload_size");
		return eh_->send_event<socket_event>(this, socket_event_flag::read, EINVAL);
	}
	else
	if (in_header_.payload_size > input_archive::max_payload_size) {
		logger_.log_u(logmsg::error, L"process::on_process_read: got too big a payload_size: %d", in_header_.payload_size);
		return eh_->send_event<socket_event>(this, socket_event_flag::read, EFBIG);
	}

	fd_t duplicated = invalid_fd;

	if (is_valid_fd(in_header_.fd) && !DuplicateHandle(fzp_.handle(), in_header_.fd, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
		logger_.log_u(logmsg::error, L"process::on_process_read: could not duplicate handle. Windows error: 0x%X", GetLastError());
		return eh_->send_event<socket_event>(this, socket_event_flag::read, EIO);
	}

	in_header_.fd = duplicated;

	in_state_ = processing_payload;

	if (must_send_read_event_ && eh_) {
		must_send_read_event_ = false;
		return eh_->send_event<socket_event>(this, socket_event_flag::read, 0);
	}
}

void fz::impersonator::process::on_process_write()
{
	waiting_for_write_event_ = false;

	while (!out_buf_.empty()) {
		auto res = fzp_.write(out_buf_.get(), out_buf_.size());

		if (res.error_ == res.wouldblock) {
			waiting_for_write_event_ = true;
			return;
		}

		if (!res) {
			logger_.log_u(logmsg::error, L"process::on_process_write: process::write() returned error: %d, raw: %d", res.error_, res.raw_);
			if (eh_)
				return eh_->send_event<socket_event>(this, socket_event_flag::write, EIO);
		}

		out_buf_.consume(res.value_);
	}

	if (must_send_write_event_ && eh_) {
		must_send_write_event_ = false;
		if (eh_)
			return eh_->send_event<socket_event>(this, socket_event_flag::write, 0);
	}
}

void fz::impersonator::process::operator()(const event_base &ev)
{
	fz::dispatch<process_event>(ev, [&](fz::process *, process_event_flag flag) {
		scoped_lock lock(mutex_);

		logger_.log_u(logmsg::debug_debug, L"%s: flag: %d, eh: %p", __PRETTY_FUNCTION__, flag, eh_);

		if (flag == process_event_flag::read)
			return on_process_read();

		if (flag == process_event_flag::write)
			return on_process_write();

		assert(false && "Invalid/unknown process_event_flag set");
		if (eh_)
			return eh_->send_event<socket_event>(this, socket_event_flag::write | socket_event_flag::read, EINVAL);
	});
}

void fz::impersonator::process::set_event_handler(event_handler *eh)
{
	scoped_lock lock(mutex_);

	if (eh_ == eh)
		return;

	eh_ = eh;
}

#endif
