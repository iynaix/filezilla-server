#include <memory>
#include <cxxabi.h>
#include <cstdlib>

#include "../util/demangle.hpp"

namespace fz::util {

std::string demangle(const char *name) {

	int status;

	std::unique_ptr<char, void(*)(void*)> res {
		abi::__cxa_demangle(name, nullptr, nullptr, &status),
		std::free
	};

	return (status==0) ? res.get() : name;
}

}
