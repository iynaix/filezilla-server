#include <utility>

#include "../util/filesystem.hpp"
#include "../logger/type.hpp"
#include "../string.hpp"

#include "../debug.hpp"

#include "backends/local_filesys.hpp"
#include "mount.hpp"
#include "validation.hpp"

#define FZ_TVFS_DEBUG_LOG FZ_DEBUG_LOG("TVFS", 0)

namespace fz::tvfs {

namespace {

static const auto are_equal = [](std::string_view a, std::string_view b) {
	constexpr bool must_be_case_insensitive = util::fs::native_format == util::fs::windows_format;

	if constexpr (must_be_case_insensitive) {
		return fz::stricmp(fz::to_wstring_from_utf8(a), fz::to_wstring_from_utf8(b)) == 0;
	}
	else {
		return a == b;
	}
};

}

const mount_tree::node *tvfs::mount_tree::nodes::find(std::string_view name) const noexcept
{
	for (const auto &v: *this) {
		if (are_equal(v.first, name)) {
			return &v.second;
		}
	}

	return nullptr;
}

mount_tree::node *tvfs::mount_tree::nodes::find(std::string_view name) noexcept
{
	return const_cast<node*>(std::as_const(*this).find(name));
}

mount_tree::node *tvfs::mount_tree::nodes::insert(std::string_view name, permissions perms)
{
	return &emplace_back(name, node{perms}).second;
}

mount_tree::node *mount_tree::nodes::prune_all_except(std::string_view name)
{
	erase(std::remove_if(begin(), end(), [&](auto &n) {
		return !are_equal(n.first, name);
	}), end());

	if (empty()) {
		return nullptr;
	}

	return &front().second;
}


std::tuple<const tvfs::mount_tree::node &, std::size_t> mount_tree::find_node(const std::vector<std::string_view> &elements) const noexcept
{
	const auto *node = &root_;
	std::size_t ei = 0;

	for (; ei < elements.size(); ++ei) {
		auto new_node = node->children.find(elements[ei]);
		if (!new_node)
			break;

		node = new_node;
	}

	return {*node, ei};
}

std::tuple<const mount_tree::node &, std::size_t, util::fs::absolute_native_path> mount_tree::resolve_path(const util::fs::absolute_unix_path &tvfs_path) const noexcept
{
	auto elements = tvfs_path.elements_view();
	auto [node, ei] = find_node(elements);
	auto node_level = elements.size() - ei;

	util::fs::absolute_native_path native_path = node.target;
	if (native_path) {
		for (;ei < elements.size(); ++ei) {
			native_path /= to_native(to_wstring_from_utf8(elements[ei]));
		}
	}

	return {node, node_level, std::move(native_path) };
}

bool mount_tree::set_root(const util::fs::absolute_unix_path &tvfs_path)
{
	if (tvfs_path) {
		auto [node, node_level, native_path] = resolve_path(tvfs_path);
		struct node new_root = node;
		root_ = std::move(new_root);
		root_.target = std::move(native_path);

		return true;
	}

	return false;
}

void mount_tree::prune_all_except(const std::vector<std::string_view> &elements)
{
	auto *node = &root_;

	for (auto e: elements) {
		node = node->children.prune_all_except(e);
		if (!node) {
			break;
		}
	}
}

mount_tree::mount_tree()
{}

mount_tree::mount_tree(const tvfs::mount_table &mt, placeholders::map map, logger_interface &logger)
{
	set_placeholders(std::move(map), logger);
	merge_with(mt, logger);
}

void mount_tree::set_placeholders(placeholders::map map, logger_interface &logger)
{
	placeholders_map_.clear();
	placeholders_map_.reserve(map.size());

	for (auto &p: map) {
		if (remove_ctrl_chars(p.first).empty()) {
			logger.log_u(logmsg::warning, L"One of the placeholders is empty or contains invalid characters. Will be ignored.");
			continue;
		}

		if (p.second.empty())
			p.second = placeholders::make_invalid_value(sprintf(fzT("The value of the placeholder %%%s is empty"), p.first));

		placeholders_map_.push_back(std::move(p));
	}
}

mount_tree &tvfs::mount_tree::merge_with(tvfs::mount_table mt, logger_interface &logger)
{
	static auto access2perms = [](mount_point::access_t access) constexpr -> permissions {
		using a = mount_point::access_t;

		switch (access) {
			case a::read_only: return permissions::read | permissions::list_mounts;
			case a::read_write: return permissions::read | permissions::list_mounts | permissions::write;
			case a::disabled: return permissions{};
		}

		return permissions{};
	};

	static auto recursive2perms = [](mount_point::recursive_t recursive) constexpr -> permissions {
		using r = mount_point::recursive_t;

		switch (recursive) {
			case r::apply_permissions_recursively: return permissions::apply_recursively;
			case r::apply_permissions_recursively_and_allow_structure_modification: return permissions::apply_recursively | permissions::allow_structure_modification;
			case r::do_not_apply_permissions_recursively: return permissions{};
		}

		return permissions{};
	};

	auto handle_validation_result = [&](std::wstring_view type, const auto &path, const validation::result &res, auto &it, std::size_t n) {
		logger.log_u(logmsg::warning, L"There have been problems while processing the mount point n. %d:", n);

		if (auto e = res.invalid_placeholder_values()) {
			logger.log_u(logmsg::warning, L"  > The placeholders expansion for the %s path \"%s\" has had issues:", type, path);
			for (const auto &x: e->explanations)
				logger.log_u(logmsg::warning, L"    > %s.", x);
		}
		else
		if (res.path_has_invalid_characters()) {
			logger.log_u(logmsg::warning, L"  > The %s path contains invalid characters.", type);
		}
		else
		if (res.path_is_not_absolute()) {
			logger.log_u(logmsg::warning, L"  > The %s path is not an absolute path.", type);
		}
		else
		if (res.path_is_empty()) {
			logger.log_u(logmsg::warning, L"  > The %s path is empty.", type);
		}

		logger.log_raw(logmsg::warning, L"This mount point will be ignored");

		it = mt.erase(it);
	};

	{
		std::size_t row = 1;
		for (auto it = mt.begin(); it != mt.end(); ++row) {
			auto &mp = *it;

			auto native_path = substitute_placeholders(mp.native_path, placeholders_map_);

			if (auto res = (mp.access == mp.disabled) ? validation::result() : validation::validate_native_path(native_path, util::fs::native_format); !res) {
				handle_validation_result(L"native", mp.native_path, res, it, row);
			}
			else
			if (res = validation::validate_tvfs_path(mp.tvfs_path); !res) {
				handle_validation_result(L"virtual", mp.tvfs_path, res, it, row);
			}
			else {
				//FIXME: this is slow. We should make the strings on the left side be paths instead
				mp.tvfs_path = util::fs::unix_path(std::move(mp.tvfs_path));
				mp.native_path = util::fs::native_path(std::move(native_path));
				++it;
			}
		}
	}

	std::sort(mt.begin(), mt.end(), [](const tvfs::mount_point &lhs, const tvfs::mount_point &rhs) {
		return util::fs::absolute_unix_path(lhs.tvfs_path).elements_view() < util::fs::absolute_unix_path(rhs.tvfs_path).elements_view();
	});

	for (const auto &mp: mt) {
		auto elements = util::fs::absolute_unix_path(mp.tvfs_path).elements();

		node *n = &root_;

		for (const auto &e: elements) {
			auto next_node = n->children.find(e);

			if (!next_node) {
				next_node = n->children.insert(e);

				if (&e != &elements.back()) {
					if (util::fs::native_path target_path = n->target; !target_path.str().empty()) {
						next_node->target = target_path / fz::to_native(e);
						if (n->perms & permissions::apply_recursively)
							next_node->perms = n->perms;
						else
							next_node->perms = permissions::list_mounts;
					}
					else
						next_node->perms = permissions::list_mounts;
				}
			}

			n = next_node;
		}

		n->target = std::move(mp.native_path);
		n->perms = access2perms(mp.access) | recursive2perms(mp.recursive);
		n->flags = mp.flags;
	}

	return *this;
}

void mount_tree::dump(const std::string &prefix, logger_interface &logger, logmsg::type level) const
{
	if (!logger.should_log(level))
		return;

	root_.dump(prefix, {}, logger, level);
}

void mount_tree::node::dump(const std::string &prefix, const std::string &root, logger_interface &logger, logmsg::type level) const
{
	logger.log_u(level, L"%s\"%s\" -> \"%s\" %s", prefix, root.empty() ? "/" : root, target, perms == permissions{} ? "(disabled)" : "");

	for (const auto &n: children)
		n.second.dump(prefix, root + "/" + n.first, logger, level);
}

static void async_autocreate_directory(mount_tree::shared_const_node n, std::shared_ptr<backend> b, receiver_handle<> r);
static void async_autocreate_directories(mount_tree::shared_const_nodes ns, std::shared_ptr<backend> b, receiver_handle<> r);

static void async_autocreate_directories(mount_tree::shared_const_nodes ns, std::shared_ptr<backend> b, receiver_handle<> r)
{
	if (ns->empty())
		return r();

	auto cur = ns->begin();

	return async_autocreate_directory(mount_tree::shared_const_node(ns, &cur->second), b, async_reentrant_receive(r)
	>> [r = std::move(r), cur, ns, b] (auto && self) mutable {
		if (++cur != ns->end())
			return async_autocreate_directory(mount_tree::shared_const_node(ns, &cur->second), b, std::move(self));

		return r();
	});
}

static void async_autocreate_directory(mount_tree::shared_const_node n, std::shared_ptr<backend> b, receiver_handle<> r)
{
	if ((n->flags & mount_point::autocreate) && !n->target.empty()) {
		return b->mkdir(n->target, true, mkdir_permissions::normal, async_receive(r)
		>> [r = std::move(r), b, n](auto) mutable {
			return async_autocreate_directories(mount_tree::shared_const_nodes(n, &n->children), std::move(b), std::move(r));
		});
	}

	return async_autocreate_directories(mount_tree::shared_const_nodes(n, &n->children), std::move(b), std::move(r));
}


void async_autocreate_directories(std::shared_ptr<mount_tree> mt, std::shared_ptr<backend> b, receiver_handle<> r)
{
	if (!mt)
		return r();

	if (!b) {
		thread_local auto static_b = std::make_shared<backends::local_filesys>(get_null_logger());
		b = static_b;
	}

	return async_autocreate_directory(mount_tree::shared_const_node(mt, &mt->root_), std::move(b), std::move(r));
}

}
