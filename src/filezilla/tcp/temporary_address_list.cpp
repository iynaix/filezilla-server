#include "temporary_address_list.hpp"

namespace fz::tcp {


temporary_address_list::temporary_address_list(fz::event_loop &loop, fz::tcp::address_list &list, fz::duration default_expiration_duration)
	: event_handler(loop)
	, list_(list)
	, default_expiration_duration_(default_expiration_duration)
{
}

temporary_address_list::~temporary_address_list()
{
	remove_handler();
}

void temporary_address_list::set_expiration_duration(duration expiration_duration)
{
	default_expiration_duration_ = expiration_duration;
}

bool temporary_address_list::contains(std::string_view address, address_type family) const
{
	return list_.contains(address, family);
}

bool temporary_address_list::add(std::string_view address, address_type family)
{
	return add(address, default_expiration_duration_, family);
}

bool temporary_address_list::remove(std::string_view address, address_type family)
{
	return list_.remove(address, family);
}

std::size_t temporary_address_list::size() const
{
	return list_.size();
}

bool temporary_address_list::add(std::string_view address, duration expiration_duration, address_type family)
{
	if (list_.add(address, family)) {
		scoped_write_lock lock(mutex_);

		auto id = add_timer(expiration_duration, true);
		map_.emplace(std::piecewise_construct,
					 std::forward_as_tuple(id),
					 std::forward_as_tuple(address, family));

		return true;
	}

	return false;
}

void temporary_address_list::operator()(const event_base &ev)
{
	dispatch<timer_event>(ev, [this](const timer_id &id) {
		auto it = map_.find(id);
		if (it != map_.end()) {
			list_.remove(it->second.first, it->second.second);

			scoped_write_lock lock(mutex_);
			map_.erase(it);
		}
	});
}

}
