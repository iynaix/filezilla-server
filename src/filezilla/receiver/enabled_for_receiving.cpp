#include <cassert>

#include "enabled_for_receiving.hpp"

namespace fz {

enabled_for_receiving_base::~enabled_for_receiving_base()
{
	assert(!get_shared_receiver_context() && "You must invoke remove_handler_and_stop_receiving() from the most derived class' destructor.");
}

}
