#ifndef FZ_RECEIVER_CONTEXT_CPP
#define FZ_RECEIVER_CONTEXT_CPP

#include "context.hpp"

namespace fz {

receiver_context::~receiver_context()
{
	while (!managed_objects_.empty()) {
		destroy_managed(static_cast<managed_object &>(managed_objects_.front()));
	}
}

}

#endif // FZ_RECEIVER_CONTEXT_CPP
