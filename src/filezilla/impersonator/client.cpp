#include <cassert>
#include <deque>

#include <libfilezilla/impersonation.hpp>

#include "client.hpp"
#include "archives.hpp"
#include "messages.hpp"
#include "process.hpp"

#include "../rmp/dispatch.hpp"

#include "../serialization/types/containers.hpp"
#include "../serialization/types/variant.hpp"
#include "../serialization/types/local_filesys.hpp"

namespace fz::impersonator {

class client::get_caller
{
public:
	get_caller(client &client)
		: client_(client)
		, caller_(get())
	{}

	callers::iterator operator->()
	{
		return *caller_;
	}

	explicit operator bool() const
	{
		return bool(caller_);
	}

	~get_caller()
	{
		if (caller_) {
			fz::scoped_lock lock(client_.mutex_);
			client_.callers_available_.splice(client_.callers_available_.end(), client_.callers_in_use_, *caller_);
			client_.condition_.signal(lock);
		}
	}

private:
	std::optional<client::callers::iterator> get()
	{
		fz::scoped_lock lock(client_.mutex_);

		if (client_.destroying_) {
			return {};
		}

		while (client_.callers_available_.empty() && client_.callers_in_use_.size() == client_.pool_size_) {
			client_.logger_.log_u(logmsg::debug_verbose, "call: All callers are busy. Waiting for one to free up.");
			client_.condition_.wait(lock);
			client_.logger_.log_u(logmsg::debug_verbose, "call: a caller just freed up.");
		}

		while (!client_.callers_available_.empty() && !client_.callers_available_.front()) {
			client_.logger_.log_u(logmsg::debug_verbose, "call: the first available caller is dead. Erasing it from the queue.");
			client_.callers_available_.pop_front();
		}

		if (client_.callers_available_.empty()) {
			client_.logger_.log_u(logmsg::debug_verbose, "call: no available callers. Let's create one.");
			client_.callers_available_.emplace_back(client_.event_loop_, client_.logger_, std::make_unique<process>(client_.event_loop_, client_.thread_pool_, client_.logger_, client_.exe_, client_.token_));
		}

		auto caller = client_.callers_available_.begin();

		// Move the caller from the front of the available queue to the back of the in use queue.
		// It will be moved to the back of the available queue in the destructor.
		client_.callers_in_use_.splice(client_.callers_in_use_.end(), client_.callers_available_, caller);

		return caller;
	}

	client &client_;
	std::optional<callers::iterator> caller_;
};

client::client(thread_pool &thread_pool, logger_interface &logger, impersonation_token &&token, native_string_view exe, std::size_t pool_size)
	: thread_pool_(thread_pool)
	, event_loop_(thread_pool_)
	, logger_(logger, "impersonator client", { { "user", token ? fz::to_utf8(token.username()) : "<invalid tocken>"} })
	, token_(std::move(token))
	, exe_(exe)
	, pool_size_(pool_size > 0 ? pool_size : 1)
{
}

client::~client()
{
	fz::scoped_lock lock(mutex_);

	destroying_ = true;

	while (!callers_in_use_.empty()) {
		logger_.log_u(logmsg::debug_verbose, "destroying: number of callers still in use: %d.", callers_in_use_.size());
		condition_.wait(lock);
	}

	logger_.log_raw(logmsg::debug_verbose, "destroying: no callers in use left.");
}

const impersonation_token &client::get_token() const
{
	return token_;
}

template <typename T, typename E, typename... Args>
auto client::call(receiver_handle<E> &&r, Args &&... args) -> decltype(std::declval<caller>().call(T(std::forward<Args>(args)...), std::move(r)))
{
	if (auto c = get_caller(*this)) {
		c->call(T(std::forward<Args>(args)...), std::move(r));
	}
	else {
		std::apply(r, messages::default_for<rmp::make_message_t<E>>()().tuple());
	}
}


/*********************/


void client::open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r)
{
	call<messages::open_file>(std::move(r), native_path, mode, flags);
}


void client::open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r)
{
	call<messages::open_directory>(std::move(r), native_path);
}

void client::rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r)
{
	call<messages::rename>(std::move(r), path_from, path_to);
}

void client::remove_file(const absolute_native_path &path, receiver_handle<remove_response> r)
{
	call<messages::remove_file>(std::move(r), path);
}

void client::remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r)
{
	call<messages::remove_directory>(std::move(r), path, recursive);
}

void client::info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r)
{
	call<messages::info>(std::move(r), path, follow_links);
}

void client::mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r)
{
	call<messages::mkdir>(std::move(r), path, recurse, permissions);
}

void client::set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r)
{
	call<messages::set_mtime>(std::move(r), path, mtime);
}

}
