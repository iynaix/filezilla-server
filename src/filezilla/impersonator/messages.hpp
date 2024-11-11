#ifndef FZ_IMPERSONATOR_MESSAGES_HPP
#define FZ_IMPERSONATOR_MESSAGES_HPP

#include <libfilezilla/local_filesys.hpp>
#include "archives.hpp"

#include "../rmp/engine.hpp"
#include "../rmp/any_message.hpp"
#include "../rmp/dispatch.hpp"
#include "../rmp/glue/receiver.hpp"
#include "../tvfs/backend.hpp"
#include "../serialization/types/fs_path.hpp"

namespace fz::impersonator::messages {
	FZ_RMP_USING_DIRECTIVES

	/***********/

	using absolute_native_path = tvfs::backend::absolute_native_path;

	using open_file = message <struct open_file_tag (absolute_native_path native_path, file::mode mode, file::creation_flags flags)>;
	using open_directory = message <struct open_directory_tag (absolute_native_path native_path)>;
	using open_response = rmp::make_message_t<tvfs::backend::open_response>;

	using rename = message <struct rename_tag (absolute_native_path path_from, absolute_native_path path_to)>;
	using rename_response = rmp::make_message_t<tvfs::backend::rename_response>;

	using remove_file = message <struct remove_file_tag (absolute_native_path path)>;
	using remove_directory = message <struct remove_directory_tag (absolute_native_path path, bool recursive)>;
	using remove_response = rmp::make_message_t<tvfs::backend::remove_response>;

	using info = message <struct info_tag (absolute_native_path path, bool follow_links)>;
	using info_response = rmp::make_message_t<tvfs::backend::info_response>;

	using mkdir = message <struct mkdir_tag (absolute_native_path path, bool recurse, mkdir_permissions permissions)>;
	using mkdir_response = rmp::make_message_t<tvfs::backend::mkdir_response>;

	using set_mtime = message <struct set_mtime_tag (absolute_native_path path, datetime mtime)>;
	using set_mtime_response = rmp::make_message_t<tvfs::backend::set_mtime_response>;

	template <typename Message>
	struct default_for
	{
		Message operator()() const {
			return {};
		}
	};

	template <>
	struct default_for<open_response>
	{
		open_response operator()() const {
			return {result{result::other}, fd_owner() };
		}
	};

	template <>
	struct default_for<rename_response>
	{
		rename_response operator()() const {
			return {result{result::other} };
		}
	};

	template <>
	struct default_for<remove_response>
	{
		remove_response operator()() const {
			return {result{result::other} };
		}
	};

	template <>
	struct default_for<info_response>
	{
		info_response operator()() const {
			return {result{result::other}, bool(), local_filesys::type(), int64_t(), datetime(), int() };
		}
	};

	template <>
	struct default_for<mkdir_response>
	{
		mkdir_response operator()() const {
			return {result{result::other} };
		}
	};

	template <>
	struct default_for<set_mtime_response>
	{
		set_mtime_response operator()() const {
			return {result{result::other} };
		}
	};
}

namespace fz::impersonator
{
	using any_message = fz::rmp::any_message<
		messages::open_file, messages::open_directory, messages::open_response,
		messages::rename, messages::rename_response,
		messages::remove_file, messages::remove_directory, messages::remove_response,
		messages::info, messages::info_response,
		messages::mkdir, messages::mkdir_response,
		messages::set_mtime, messages::set_mtime_response
	>;
}

#endif // FZ_IMPERSONATOR_MESSAGES_HPP
