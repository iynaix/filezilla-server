#include "handle.hpp"

namespace fz {

receiver_handle_base::~receiver_handle_base()
{
	if (r_) {
		if (managed_) {
			if (auto &&c = c_.lock()) {
				r_->done_in(*c);
			}
		}
		else {
			delete r_;
		}
	}
}

}
