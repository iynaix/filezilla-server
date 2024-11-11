#include <memory>
#include <cerrno>

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/local_filesys.hpp>

#include "io.hpp"
#include "filesystem.hpp"
#include "../logger/file.hpp"

namespace fz::util::io {

namespace {

int get_errno() {
#ifdef FZ_WINDOWS
	return EIO;
#else
	return errno;
#endif
}

}


bool write(file &file, const void *data, std::size_t size, int *error)
{
	if (!file.opened()) {
		if (error)
			*error = EBADF;

		return false;
	}

	std::string_view view(reinterpret_cast<const char*>(data), size);

	while (view.size()) {
		int64_t to_write = 0;

		if constexpr (sizeof(std::size_t) >= sizeof(int64_t))
			to_write = int64_t(std::min(view.size(), std::size_t(std::numeric_limits<int64_t>::max())));
		else
			to_write = int64_t(view.size());

		auto written = file.write(view.data(), to_write);
		if (written < 0) {
			if (error)
				*error = get_errno();

			return false;
		}

		view.remove_prefix(size_t(written));
	}

	if (error)
		*error = 0;

	return true;
}

bool write(file &file, const buffer &b, int *error)
{
	return write(file, b.get(), b.size(), error);
}

bool read(file &file, void *data, std::size_t size, int *error, std::size_t *effective_read)
{
	if (!file.opened()) {
		if (error)
			*error = EBADF;

		return false;
	}

	auto orig_size = size;

	while (size > 0) {
		int64_t to_read;

		if constexpr (sizeof(std::size_t) >= sizeof(int64_t))
			to_read = int64_t(std::min(size, std::size_t(std::numeric_limits<int64_t>::max())));
		else
			to_read = int64_t(size);

		auto read = file.read(data, to_read);

		if (read < 0) {
			if (error)
				*error = get_errno();

			if (effective_read)
				*effective_read = orig_size - size;

			return false;
		}

		if (read == 0) {
			if (error)
				*error = ENODATA;

			if (effective_read)
				*effective_read = orig_size - size;

			return false;
		}

		size -= size_t(read);
		data = reinterpret_cast<char *>(data) + size_t(read);
	}

	if (effective_read)
		*effective_read = orig_size - size;

	return true;
}

bool read(file &file, buffer &b, int *error)
{
	static constexpr std::size_t chunk_size = 128*1024;

	int err{};
	std::size_t effective_read{};

	for (bool success = true; success; b.add(effective_read))
		success = read(file, b.get(chunk_size), chunk_size, &err, &effective_read);

	if (err == ENODATA) {
		if (error)
			*error = 0;

		return true;
	}

	if (error)
		*error = err;

	return false;
}

buffer read(file &file, int *error)
{
	buffer b;

	read(file, b, error);

	return b;
}

bool copy(file &in, file &out, int *error)
{
	static constexpr std::size_t chunk_size = 128*1024;

	int err{};
	std::size_t effective_read{};
	fz::buffer b;

	while (true) {
		bool success = read(in, b.get(chunk_size), chunk_size, &err, &effective_read);
		b.add(effective_read);

		if (effective_read) {
			if (!write(out, b, &err))
				break;

			b.consume(effective_read);
		}

		if (!success) {
			if (err == ENODATA)
				err = 0;

			break;
		}
	}

	if (error)
		*error = err;

	return err == 0;
}

bool copy(file &&in, file &&out, int *error)
{
	fz::file own_in = std::move(in);
	fz::file own_out = std::move(out);

	return copy(own_in, own_out, error);
}

bool copy(native_string_view in, native_string_view out, file::creation_flags flags, int *error)
{
	fz::file in_file(native_string(in), fz::file::mode::reading);
	fz::file out_file(native_string(out), fz::file::mode::writing, flags | file::creation_flags::empty);

	return copy(in_file, out_file, error);
}

bool copy_dir(native_string_view in, native_string_view out, mkdir_permissions permissions, int *error)
{
	if (auto res = fz::mkdir(native_string(out), true, permissions); !res) {
		if (error) {
			switch (res.error_) {
				case res.nodir: *error = ENOTDIR; break;
				case res.invalid: *error = EINVAL; break;
				case res.nofile: *error = ENOENT; break;
				case res.nospace: *error = ENOSPC; break;
				case res.noperm: *error = EACCES; break;
				case res.other: *error = res.raw_; break;
				case res.ok: *error = 0; break;
				case res.resource_limit: *error = EMFILE; break;
			}
		}

		return false;
	}

	local_filesys in_dir;
	in_dir.begin_find_files(native_string(in), false, false);

	native_string name;
	bool is_link;
	local_filesys::type type;

	bool success = true;

	while (in_dir.get_next_file(name, is_link, type, nullptr, nullptr, nullptr)) {
		auto src = fs::native_path(in) / name;
		auto dst = fs::native_path(out) / name;

		if (type == local_filesys::dir) {
			success &= copy_dir(src, dst, permissions, error);
		}
		else
		if (type == local_filesys::file) {
			auto cflags = file::creation_flags::empty;

			if (permissions == mkdir_permissions::cur_user)
				cflags = cflags | file::creation_flags::current_user_only;
			else
			if (permissions == mkdir_permissions::cur_user_and_admins)
				cflags = cflags | file::creation_flags::current_user_and_admins_only;

			success &= io::copy(src, dst, cflags, error);
		}
	}

	return success;
}

}
