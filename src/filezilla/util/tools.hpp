#ifndef FZ_UTIL_TOOLS_HPP
#define FZ_UTIL_TOOLS_HPP

#include "filesystem.hpp"

namespace fz::util {

fs::native_path get_own_executable_directory();
fs::native_path find_tool(fz::native_string name, fs::native_path build_rel_path, const char *env);
fs::absolute_native_path get_current_directory_name();

/// \returns the absolute top src dir, if own executable dir is a build dir
/// otherwise it returns an invalid path
fs::absolute_native_path get_abs_top_src_directory(fs::relative_native_path top_srcdir);

}

#endif // FZ_UTIL_TOOLS_HPP
