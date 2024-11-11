#ifndef FZ_TVFS_BACKEND_HPP
#define FZ_TVFS_BACKEND_HPP

#if !defined(FZ_WINDOWS)
#	include <unistd.h>
#endif

#include <libfilezilla/file.hpp>
#include <libfilezilla/local_filesys.hpp>

#include "../util/filesystem.hpp"
#include "../receiver.hpp"

namespace fz::tvfs {

using fd_t = file::file_t;

#if defined(FZ_WINDOWS)
	inline const fd_t invalid_fd = INVALID_HANDLE_VALUE;
	inline bool is_valid_fd(fd_t fd)
	{
		return fd != invalid_fd;
	}
#else
	inline constexpr fd_t invalid_fd = -1;
	inline constexpr bool is_valid_fd(fd_t fd)
	{
		return fd >= 0;
	}
#endif

class fd_owner final
{
public:
	fd_owner(file && f) noexcept
		: fd_(f.detach())
	{
	}

	fd_owner() noexcept
		: fd_(invalid_fd)
	{}

	explicit fd_owner(fd_t fd) noexcept
		: fd_(fd)
	{
	}

	fd_owner(fd_owner &&rhs) noexcept
		: fd_(rhs.release())
	{
	}

	fd_owner &operator=(fd_owner &&rhs) noexcept
	{
		fd_ = rhs.release();
		return *this;
	}

	fd_owner(const fd_owner &) = delete;
	fd_owner &operator=(const fd_owner &) = delete;

	explicit operator bool() const
	{
		return is_valid_fd(fd_);
	}

	bool is_valid() const
	{
		return is_valid_fd(fd_);
	}

	~fd_owner() noexcept
	{
		reset();
	}

	[[nodiscard]] fd_t get() const noexcept
	{
		return fd_;
	}

	[[nodiscard]] fd_t release() noexcept
	{
		return std::exchange(fd_, invalid_fd);
	}

	void reset() noexcept
	{
		if (!is_valid_fd(fd_))
			return;

	#if defined(FZ_WINDOWS)
		::CloseHandle(fd_);
	#else
		::close(fd_);
	#endif

		fd_ = invalid_fd;
	}

private:
	fd_t fd_;
};

class backend
{
public:
	virtual ~backend();

	struct open_response_tag{};
	struct rename_response_tag{};
	struct remove_response_tag{};
	struct info_response_tag{};
	struct mkdir_response_tag{};
	struct set_mtime_response_tag{};

	using open_response = receiver_event<open_response_tag, result, fd_owner>;
	using rename_response = receiver_event<rename_response_tag, result>;
	using remove_response = receiver_event<remove_response_tag, result>;
	using info_response = receiver_event<info_response_tag, result, bool, local_filesys::type, int64_t, datetime, int>;
	using mkdir_response = receiver_event<mkdir_response_tag, result>;
	using set_mtime_response = receiver_event<set_mtime_response_tag, result>;

	using absolute_native_path = util::fs::absolute_native_path;

	virtual void open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r) = 0;
	virtual void open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r) = 0;
	virtual void rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r) = 0;
	virtual void remove_file(const absolute_native_path &path, receiver_handle<remove_response> r) = 0;
	virtual void remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r) = 0;
	virtual void info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r) = 0;
	virtual void mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r) = 0;
	virtual void set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r) = 0;
};


}

#endif // FZ_TVFS_BACKEND_HPP
