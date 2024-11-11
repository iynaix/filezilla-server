#ifndef FZ_UTIL_PROOF_OF_WORK_HPP
#define FZ_UTIL_PROOF_OF_WORK_HPP

#include <string>

#include <libfilezilla/uri.hpp>

namespace fz::util {

query_string proof_of_work(const std::string &name, std::size_t difficulty, std::initializer_list<std::pair<std::string /*name*/, std::string /*value*/>> params);

}

#endif // FZ_UTIL_PROOF_OF_WORK_HPP
