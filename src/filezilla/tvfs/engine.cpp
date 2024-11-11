#include "engine.hpp"
#include "backends/local_filesys.hpp"
#include "../strresult.hpp"

namespace fz::tvfs {

result engine::open_file(file_holder &out_file, std::string_view tvfs_path, file::mode mode, int64_t rest)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_open_file(out_file, tvfs_path, mode, rest, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

result engine::get_entries(entries_iterator &out_iterator, std::string_view tvfs_path, traversal_mode mode)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_get_entries(out_iterator, tvfs_path, mode, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

std::pair<result, entry> engine::get_entry(std::string_view tvfs_path)
{
	std::pair<result, entry> out { { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) }, {} };

	async_get_entry(tvfs_path, timeout_receive_ >> out);

	return out;
}

std::pair<result, std::string> engine::make_directory(std::string tvfs_path)
{
	std::pair<result, std::string> out;

	async_make_directory(tvfs_path, timeout_receive_ >> out);

	if (!timeout_receive_) {
		out = { { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) }, resolve_path(tvfs_path).tvfs_path };
	}

	return out;
}

std::pair<result, entry> engine::set_mtime(std::string_view tvfs_path, entry_time mtime)
{
	std::pair<result, entry> out = { { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) }, {} };

	async_set_mtime(tvfs_path, mtime, timeout_receive_ >> out);

	return out;
}

result engine::remove_file(std::string_view tvfs_path)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_remove_file(tvfs_path, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

result engine::remove_directory(std::string_view tvfs_path, bool recursive)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_remove_directory(tvfs_path, recursive, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

result engine::remove_entry(const entry &entry)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_remove_entry(entry, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

result engine::rename(std::string_view from, std::string_view to)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_rename(from, to, timeout_receive_ >> std::tie(res, std::ignore));

	return res;
}

result engine::set_current_directory(std::string_view tvfs_path)
{
	result res = { result::other, FZ_RESULT_RAW(ERROR_TIMEOUT, ETIMEDOUT) };

	async_set_current_directory(tvfs_path, timeout_receive_ >> std::tie(res));

	return res;
}

void engine::async_open_file(file_holder &out_file, std::string_view tvfs_path, file::mode mode, int64_t rest, receiver_handle<completion_event> r)
{
	auto resolved_path = resolve_path(tvfs_path);

	if (!resolved_path)
		return r(result{result::invalid}, tvfs_path);

	if (open_limits_.files != open_limits::unlimited && open_files_counter_.count() >= open_limits_.files) {
		logger_.log_u(logmsg::debug_warning, L"Cannot open any more files, limit reached. Quota: %d", open_limits_.files);
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));
	}

	if ((mode == file::mode::writing || mode == file::mode::readwrite) && !(resolved_path.node.perms & permissions::write))
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));

	if ((mode == file::mode::reading || mode == file::mode::readwrite) && !(resolved_path.node.perms & permissions::read))
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));

	// We must take great care not to std::move objects into the lambda if they're also used in the function call itself,
	// as order of evaluation is unspecified.
	// Moving the receiver_handle is safe, because it's used only by async_receive(), which sits on the left side of the
	// >> operator, which evaluates left-to-right: so first async_receive(r) takes place, then std::move(r).
	return backend_->open_file(resolved_path.native_path, mode, rest == 0 ? file::empty : file::existing, async_receive(r)
	>> [this, &out_file, r = std::move(r), path = std::move(resolved_path.tvfs_path), rest, mode](auto res, auto &fd) mutable {
		if (!res)
			return r(res, std::move(path));

		if (out_file = {file(fd.release()), open_files_counter_, {}}; !out_file)
			return r(result{result::nofile}, path);

		if (rest == rest_mode::append)
			rest = out_file->size();

		if (rest > 0) {
			if (
				(rest > out_file->size())

				||

				(out_file->seek(rest, file::seek_mode::begin) == -1)

				||

				((mode == file::mode::writing || mode == file::mode::readwrite) && !out_file->truncate())
			) {
				out_file = {};
				return r(result{result::other}, path);
			}
		}

		return r(result{result::ok}, path);
	});
}

void engine::async_get_entries(entries_iterator &out_iterator, std::string_view tvfs_path, traversal_mode mode, receiver_handle<completion_event> r)
{
	auto resolved_path = resolve_path(tvfs_path);

	if (!resolved_path)
		return r(result{result::invalid}, tvfs_path);

	out_iterator.async_begin_iteration(mode, std::move(resolved_path), backend_, open_directories_counter_, open_limits_.directories, logger_, std::move(r));
}

void engine::async_get_entry(std::string_view tvfs_path, receiver_handle<entry_result> r)
{
	resolve_path(tvfs_path).async_to_entry(backend_, std::move(r));
}

void engine::async_make_directory(std::string tvfs_path, receiver_handle<completion_event> r)
{
	auto resolved_path = resolve_path(tvfs_path);

	if (!resolved_path)
		return r(result{result::invalid}, std::move(tvfs_path));

	if (!(resolved_path.node.perms & permissions::allow_structure_modification))
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));

	return backend_->mkdir(resolved_path.native_path, false, mkdir_permissions::normal, async_receive(r)
	>> [r = std::move(r), path = std::move(resolved_path.tvfs_path)](auto res) {
		return r(res, std::move(path));
	});
}

void engine::async_set_mtime(std::string_view tvfs_path, entry_time mtime, receiver_handle<entry_result> r)
{
	async_get_entry(tvfs_path, async_receive(r) >> [this, r = std::move(r), mtime](auto res, auto &e) mutable {
		if (!res)
			return r(res, std::move(e));

		if (!(e.perms_ & permissions::write))
			return r(result{result::noperm}, std::move(e));

		auto native_name = e.native_name_;
		return backend_->set_mtime(native_name, mtime, async_receive(r) >> [r = std::move(r), e = std::move(e), mtime](auto res) mutable {
			if (res)
				e.mtime_ = mtime;

			return r(res, std::move(e));
		});
	});
}

void engine::async_remove_file(std::string_view tvfs_path, receiver_handle<completion_event> r)
{
	auto resolved_path = resolve_path(tvfs_path);

	if (!resolved_path)
		return r(result{result::invalid}, std::move(tvfs_path));

	if (!(resolved_path.node.perms & permissions::remove))
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));

	return backend_->remove_file(resolved_path.native_path, async_receive(r)
	>> [r = std::move(r), path = std::move(resolved_path.tvfs_path)] (auto res) {
		r(res, std::move(path));
	});
}

void engine::async_remove_directory(std::string_view tvfs_path, bool recursive, receiver_handle<completion_event> r)
{
	auto resolved_path = resolve_path(tvfs_path);

	if (!resolved_path)
		return r(result{result::invalid}, std::move(tvfs_path));

	if (!(resolved_path.node.perms & permissions::remove))
		return r(result{result::noperm}, std::move(resolved_path.tvfs_path));

	return backend_->remove_directory(resolved_path.native_path, recursive, async_receive(r)
	>> [r = std::move(r), path = std::move(resolved_path.tvfs_path)] (auto res) {
		r(res, std::move(path));
	});
}

void engine::async_remove_entry(entry entry, receiver_handle<completion_event> r)
{
	if (!(entry.perms_ & permissions::remove))
		return r(result{result::noperm}, entry.name());

	if (entry.type_ == entry_type::file)
		return backend_->remove_file(entry.native_name_, async_receive(r)
		>> [r = std::move(r), path = std::move(entry.name_)] (auto res) {
			r(res, std::move(path));
		});
	else
	if (entry.type_ == entry_type::dir)
		return backend_->remove_directory(entry.native_name_, false, async_receive(r)
		>> [r = std::move(r), path = std::move(entry.name_)] (auto res) {
			r(res, std::move(path));
		});

	return r(result{result::other}, entry.name());
}

void engine::async_rename(std::string_view from, std::string_view to, receiver_handle<completion_event> r)
{
	auto resolved_from = resolve_path(from);
	auto resolved_to = resolve_path(to);

	if (!resolved_from)
		return r(result{result::invalid}, from);

	if (!resolved_to)
		return r(result{result::invalid}, to);

	if (!(resolved_from.node.perms & permissions::rename) || !(resolved_to.node.perms & permissions::rename))
		return r(result{result::noperm}, resolved_from.tvfs_path);

	return backend_->rename(resolved_from.native_path, resolved_to.native_path, async_receive(r)
	>> [r = std::move(r), path = std::move(resolved_from.tvfs_path)](auto res) {
		r(res, std::move(path));
	});
}

void engine::async_set_current_directory(std::string_view tvfs_path, receiver_handle<simple_completion_event> r)
{
	resolve_path(tvfs_path).async_to_entry(backend_, async_receive(r) >> [this, r = std::move(r)](result res, entry &e) mutable {
		if (!res)
			return r(res);

		if (!e.is_directory())
			return r(result{result::nodir});

		if (!(e.perms_ & (permissions::read | permissions::list_mounts)))
			return r(result{result::noperm});

		current_directory_ = e.name();

		r(result{result::ok});
	});
}

const util::fs::absolute_unix_path &engine::get_current_directory() const
{
	return current_directory_;
}

engine::engine(logger_interface &logger, duration sync_timeout)
	: logger_(logger, "TVFS")
	, timeout_receive_{sync_timeout}
	, current_directory_("/")
{
	set_mount_tree(nullptr);
	set_backend(nullptr);
}

void engine::set_mount_tree(std::shared_ptr<mount_tree> mt) noexcept
{
	mount_tree_ = mt ? std::move(mt) : std::make_shared<mount_tree>();
}

void engine::set_backend(std::shared_ptr<backend> backend) noexcept
{
	backend_ = backend ? std::move(backend) : std::make_shared<backends::local_filesys>(logger_);
}

void engine::set_open_limits(const open_limits &limits)
{
	open_limits_ = limits;
}

resolved_path engine::resolve_path(std::string_view path)
{
	auto canonical_path = current_directory_ / path;

	if (!canonical_path)
		return {};

	auto [node, node_level, native_path] = mount_tree_->resolve_path(canonical_path);
	auto perms = node.perms;

	if (!(perms & permissions::apply_recursively) && node_level > 1)
		perms = {};

	mount_tree::shared_const_nodes children{};
	if (node_level == 0)
		children = mount_tree::shared_const_nodes(mount_tree_, &node.children);
	else
	if (perms & permissions::write)
		perms |= permissions::remove | permissions::rename;

	return { std::move(canonical_path), std::move(native_path), { perms, std::move(children)} };
}

file_holder::file_holder(file &&file, const util::copies_counter &counter, badge<engine>)
	: file_(std::move(file))
	, counter_(counter)
{

}



}
