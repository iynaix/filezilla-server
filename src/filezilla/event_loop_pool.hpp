#ifndef FZ_EVENT_LOOP_POOL_HPP
#define FZ_EVENT_LOOP_POOL_HPP

#include <libfilezilla/event_loop.hpp>
#include <libfilezilla/thread_pool.hpp>

namespace fz {

class event_loop_pool
{
public:
	event_loop_pool(event_loop &main_loop, thread_pool &pool, std::uint32_t max_num_of_loops = 0);

	void set_max_num_of_loops(std::uint32_t max);
	event_loop &get_loop();

private:
	fz::mutex mutex_;

	event_loop &main_loop_;
	thread_pool &pool_;
	std::uint32_t max_num_of_loops_;
	std::vector<std::unique_ptr<event_loop>> loops_;
};

}

#endif // FZ_EVENT_LOOP_POOL_HPP
