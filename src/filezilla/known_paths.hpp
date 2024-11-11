#ifndef FZ_KNOWN_PATHS_HPP
#define FZ_KNOWN_PATHS_HPP

#include "../filezilla/util/filesystem.hpp"

namespace fz::known_paths {

using path = util::fs::native_path;
using path_list = util::fs::native_path_list;

namespace detail {

	struct base
	{
		const path &for_writing() const
		{
			return path_for_writing_;
		}

		const path_list &for_reading() const
		{
			return paths_for_reading_;
		}

	protected:
		explicit base(const path &path_for_writing, const path_list &paths_for_reading);

		struct file {
			file (base &owner, path && name)
				: owner_(owner)
				, name_(std::move(name))
			{}

			auto operator()(fz::file::mode mode) const
			{
				if (mode == fz::file::reading)
					return owner_.paths_for_reading_.resolve(name_, mode, fz::file::current_user_and_admins_only);

				return owner_.path_for_writing_.resolve(name_, mode, fz::file::existing | fz::file::current_user_and_admins_only);
			}

		private:
			base &owner_;
			path name_;
		};

		struct dir {
			dir (base &owner, path && name)
				: owner_(owner)
				, name_(std::move(name))
			{}

			auto operator()() const
			{
				return owner_.path_for_writing_ / name_;
			}

		private:
			base &owner_;
			path name_;
		};

		file f(known_paths::path && name)
		{
			return {*this, std::move(name)};
		}

		dir d(known_paths::path && name)
		{
			return {*this, std::move(name)};
		}

	private:
		known_paths::path path_for_writing_;
		known_paths::path_list paths_for_reading_;
	};
}

struct data: detail::base {
	data(const path &dir_name)
		: base(
			effective_home() / dir_name,
			(effective_home() + dirs()) / dir_name
		)
	{}

	data(const path &additional_path_for_reading, const path &dir_name)
		: base(
			effective_home() / dir_name,
			(effective_home() + additional_path_for_reading + dirs()) / dir_name
		)
	{}

	static const path &home();
	static const path_list &dirs();

private:
	static const path &effective_home();
};

struct config: detail::base {
	config(const path &dir_name)
		: base(
			effective_home() / dir_name,
			(effective_home() + dirs()) / dir_name
		)
	{}

	static const path &home();
	static const path_list &dirs();

private:
	static const path &effective_home();
};

#if defined(FZ_WINDOWS)
namespace windows {
	const path &program_data();
	const path &windir();
}
#endif

namespace user {
	const path &home();
	const path &downloads();
}

#ifdef FZ_KNOWN_PATHS_CONFIG_FILE
#   error FZ_KNOWN_PATHS_CONFIG_FILE already defined.
#endif

#ifdef FZ_KNOWN_PATHS_CONFIG_DIR
#   error FZ_KNOWN_PATHS_CONFIG_DIR already defined.
#endif

#define FZ_KNOWN_PATHS_CONFIG_FILE(name) file name = f(fzT(#name) fzT(".xml"))
#define FZ_KNOWN_PATHS_CONFIG_DIR(name) dir name = d(fzT(#name))

}

#endif // FZ_XDG_HPP
