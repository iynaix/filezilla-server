#include <libfilezilla/libfilezilla.hpp>

#if !defined(FZ_WINDOWS)
#	include <unistd.h>
#endif

#include "util.hpp"

namespace fz::impersonator {

bool can_impersonate()
{
	// This is just a (poor) heuristic. We should ideally check for proper capabilities
#if defined(FZ_UNIX) || defined (FZ_MAC)
	return geteuid() == 0;
#else
	return true;
#endif

}

}
