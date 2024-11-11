#ifndef FZ_TVFS_MOUNT_HPP
#define FZ_TVFS_MOUNT_HPP

#include <string>
#include <memory>

#include <libfilezilla/string.hpp>
#include <libfilezilla/logger.hpp>

#include "permissions.hpp"
#include "backend.hpp"
#include "placeholders.hpp"

#include "../enum_bitops.hpp"

namespace fz::tvfs {

struct mount_point {
	std::string tvfs_path;
	native_string native_path;

	enum access_t: std::uint8_t {
		read_only,
		read_write,
		disabled
	} access = read_write;

	enum recursive_t: std::uint8_t {
		do_not_apply_permissions_recursively,
		apply_permissions_recursively,
		apply_permissions_recursively_and_allow_structure_modification
	} recursive = apply_permissions_recursively_and_allow_structure_modification;

	enum flags_t: std::uint8_t {
		autocreate = 1
	} flags = {};

	FZ_ENUM_BITOPS_FRIEND_DEFINE_FOR(flags_t)

	// The following was introduced after 1.7.2, with the introduction of a new placeholders format,
	// in order to make it easier to rollback a faulty release.
	// The idea is that we keep around both the old and new path, with different placeholders,
	// for as long as they are still equivalent. If the path gets changed, then the old one gets deleted and the hability to rollback is lost beyond repair.
	// Once we're sure a new release is stable enough, the old native path will be ditched from this structure.
	native_string old_native_path = {};
};

struct mount_table: std::vector<mount_point> {
	using std::vector<mount_point>::vector;
};

class mount_tree {
public:
	struct node;
	struct nodes: std::vector<std::pair<std::string, node>> {
		using std::vector<std::pair<std::string, node>>::vector;

		const node *find(std::string_view name) const noexcept;
		node *find(std::string_view name) noexcept;
		node *insert(std::string_view name, permissions perms = {});

		node *prune_all_except(std::string_view name);
	};

	using shared_const_node = std::shared_ptr<const node>;
	using shared_const_nodes = std::shared_ptr<const nodes>;

	struct node {
		nodes children{};
		native_string target{};
		permissions perms{};
		mount_point::flags_t flags{};

		node(permissions perms = {})
			: perms(perms)
		{}

		void dump(const std::string &prefix, const std::string &root, logger_interface &logger, logmsg::type level) const;
	};

	std::tuple<const node &, std::size_t /* remainder start pos */> find_node(const std::vector<std::string_view> &elements) const noexcept;
	std::tuple<const node &, std::size_t /* node_level */, util::fs::absolute_native_path> resolve_path(const util::fs::absolute_unix_path &tvfs_path) const noexcept;
	bool set_root(const util::fs::absolute_unix_path &tvfs_path);

	void prune_all_except(const std::vector<std::string_view> &elements);

	mount_tree();
	mount_tree(const mount_table &mt, placeholders::map placeholders = {}, logger_interface &logger = get_null_logger());

	mount_tree &merge_with(mount_table mt, logger_interface &logger = get_null_logger());
	void set_placeholders(placeholders::map placeholders, logger_interface &logger = get_null_logger());

	void dump(const std::string &prefix, logger_interface &logger, logmsg::type level) const;

private:
	friend void async_autocreate_directories(std::shared_ptr<mount_tree> mt, std::shared_ptr<backend> b, receiver_handle<> r);
	node root_{permissions::list_mounts};
	placeholders::map placeholders_map_;
};

void async_autocreate_directories(std::shared_ptr<mount_tree> mt, std::shared_ptr<backend> b, receiver_handle<> r);

}

#endif // FZ_TVFS_MOUNT_HPP
