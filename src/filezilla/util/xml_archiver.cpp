#include "xml_archiver.hpp"
#include "remove_event.hpp"

namespace fz::util {

xml_archiver_base::xml_archiver_base(fz::event_loop &loop, fz::duration delay, fz::mutex *mutex, fz::event_handler *target_handler)
	: fz::event_handler(loop)
	, target_handler_(target_handler)
	, delay_(delay)
	, mutex_(mutex)
{}

xml_archiver_base::~xml_archiver_base()
{
}

void xml_archiver_base::save_later()
{
	scoped_maybe_locker lock(mutex_);

	if (!timer_id_)
		timer_id_ = add_timer(delay_, true);
}

void xml_archiver_base::set_saving_delay(duration delay)
{
	scoped_maybe_locker lock(mutex_);

	delay_ = delay;
}

void xml_archiver_base::set_event_handler(event_handler *target_handler)
{
	scoped_maybe_locker lock(mutex_);

	if (target_handler != target_handler_) {
		if (target_handler_)
			//FIXME: this should actually be a move_events (without the re-), to the new handler
			remove_events<archive_result>(target_handler_, *this);

		target_handler_ = target_handler;
	}
}

void xml_archiver_base::operator()(const event_base &ev)
{
	fz::dispatch<fz::timer_event>(ev, [this](fz::timer_id){
		{
			scoped_maybe_locker lock(mutex_);
			timer_id_ = 0;
		}

		save_now(delayed_dispatch);
	});
}

void xml_archiver_base::dispatch_event(xml_archiver_base::event_dispatch_mode mode, const xml_archiver_base::archive_info &info, int error)
{
	if (target_handler_) {
		if (mode == direct_dispatch)
			target_handler_->operator()(archive_result(*this, info, error));
		else
		if (mode == delayed_dispatch)
			target_handler_->send_event<archive_result>(*this, info, error);
	}
}


}
