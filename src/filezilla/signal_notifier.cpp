#include <unistd.h>
#include <signal.h>

#include <libfilezilla/mutex.hpp>
#include <libfilezilla/thread.hpp>
#include <libfilezilla/glue/unix.hpp>

#include <unordered_map>
#include <list>

#include "signal_notifier.hpp"

namespace fz {

namespace {

	template <typename T, std::enable_if_t<std::is_trivial_v<T>>* = nullptr>
	bool write(int fd, const T &value)
	{
		auto *buf = reinterpret_cast<const char *>(&value);
		auto size = sizeof(T);

		size_t written_size = 0;
		while (written_size < size) {
			auto chunk_size = ::write(fd, buf + written_size, size - written_size);
			if (chunk_size < 0) {
				if (errno == EINTR)
					continue;

				return false;
			}
			else
			if (chunk_size == 0)
				return false;

			written_size += size_t(chunk_size);
		}

		return true;
	}

	template <typename T, std::enable_if_t<std::is_trivial_v<T>>* = nullptr>
	bool read(int fd, T &value)
	{
		auto *buf = reinterpret_cast<char *>(&value);
		auto size = sizeof(T);

		size_t read_size = 0;
		while (read_size < size) {
			auto chunk_size = ::read(fd, buf + read_size, size - read_size);
			if (chunk_size < 0) {
				if (errno == EINTR)
					continue;

				return false;
			}
			if (chunk_size == 0)
				return false;

			read_size += size_t(chunk_size);
		}

		return true;
	}

	struct data
	{
		~data() {
			cleanup();
		}

		void close_pipe() {
			if (fds_[1] >= 0)
				::close(fds_[1]);

			if (fds_[0] >= 0)
				::close(fds_[0]);

			fds_[1] = -1;
			fds_[0] = -1;
		}

		void cleanup() {
			if (thread_.joinable()) {
				if (fds_[1] >= 0)
					write(fds_[1], int(-1));

				thread_.join();
			}

			// Signals handlers can still get called during destruction of globals,
			// hence we need to restore the old handlers.
			for (auto & handler : handlers_) {
				if (signal(handler.first, handler.second.old_posix_handler) == SIG_ERR) {
					// Restoring signal handlers failed, cannot continue normally.
					_exit(1);
				}
			}
			handlers_.clear();

			close_pipe();
		}

		bool setup() {
			if (fds_[0] < 0 && !fz::create_pipe(fds_))
				return false;

			if (thread_.joinable())
				return true;

			fz::mutex wait_mutex;
			fz::condition wait_condition;

			auto started = thread_.run([this, &wait_mutex, &wait_condition]{
				// Block all signals in this thread, we don't want to be interrupted.
				sigset_t all_signals;
				sigfillset(&all_signals);
				pthread_sigmask(SIG_SETMASK, &all_signals, nullptr);

				{
					fz::scoped_lock wait_lock(wait_mutex);
					wait_condition.signal(wait_lock);
				}

				int signum = 0;

				while (read(fds_[0], signum)) {
					if (signum < 0) {
						return;
					}

					fz::scoped_lock lock(mutex_);

					if (auto it = handlers_.find(signum); it != handlers_.end()) {
						for (const auto &h: it->second.list)
							h(signum);
					}
				}
			});

			if (started) {
				fz::scoped_lock wait_lock(wait_mutex);
				wait_condition.wait(wait_lock);
			}

			return started;
		}

		fz::thread thread_{};
		fz::mutex mutex_{false};
		int fds_[2]{-1, -1};

		struct handlers_data {
			void (*old_posix_handler)(int);
			signal_notifier::handlers_list list;
		};

		std::unordered_map<int /*signum*/, handlers_data> handlers_{};

		int current_signum{};
	};

	data data;

}

signal_notifier::token_t fz::signal_notifier::attach(int signum, fz::signal_handler_t handler)
{
	fz::scoped_lock lock(data.mutex_);

	if (signum <= 0 || !data.setup())
		return {};

	if (auto it = data.handlers_.find(signum); it == data.handlers_.end()) {
		auto old_handler = ::signal(signum, [](int signum) {
			 write(data.fds_[1], signum);
		});

		if (old_handler != SIG_ERR) {
			it = data.handlers_.insert({signum, {old_handler, {}}}).first;
			goto found;
		}
	}
	else found: {
		it->second.list.push_back(std::move(handler));
		return { signum, std::prev(it->second.list.end()) };
	}

	return {};
}

bool signal_notifier::detach(fz::signal_notifier::token_t &token)
{
	scoped_lock lock(data.mutex_);

	if (auto it = data.handlers_.find(token.signum); it != data.handlers_.end()) {
		it->second.list.erase(token.handler_it);

		if (it->second.list.empty()) {
			signal(it->first, it->second.old_posix_handler);
			data.handlers_.erase(it);
		}

		if (data.handlers_.empty())
			data.cleanup();

		token.signum = {};
		return true;
	}

	return false;
}

}
