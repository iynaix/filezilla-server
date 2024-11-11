#include <libfilezilla/util.hpp>

#include "event_loop_pool.hpp"

namespace fz {

event_loop_pool::event_loop_pool(fz::event_loop &main_loop, fz::thread_pool &pool, uint32_t max_num_of_loops)
	: main_loop_(main_loop)
	, pool_(pool)
{
	set_max_num_of_loops(max_num_of_loops);
}

void event_loop_pool::set_max_num_of_loops(std::uint32_t max)
{
	scoped_lock lock(mutex_);

	max_num_of_loops_ = max;

	if (max_num_of_loops_ > 0) {
		loops_.reserve(max_num_of_loops_);

		while (loops_.size() < max_num_of_loops_) {
			loops_.push_back(std::make_unique<event_loop>(pool_));
		}
	}
}

event_loop &event_loop_pool::get_loop()
{
	if (max_num_of_loops_ > 0)
		return *loops_[std::uint32_t(fz::random_number(0, max_num_of_loops_-1))].get();

	return main_loop_;
}

}





