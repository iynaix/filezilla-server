#include <vector>

#include "autobanner.hpp"
#include "../hostaddress.hpp"

namespace fz::authentication {

autobanner::autobanner(event_loop &loop, options opts)
	: event_handler(loop)
{
	set_options(std::move(opts));
}

autobanner::~autobanner()
{
	remove_handler();
}

void autobanner::set_options(options opts)
{
	scoped_lock lock(mutex_);
	opts_ = std::move(opts);
}

void autobanner::add_event_handler(event_handler &handler)
{
	scoped_lock lock(mutex_);
	if (auto it = std::find(handlers_.begin(), handlers_.end(), &handler); it == handlers_.end())
		handlers_.push_back(&handler);
}

void autobanner::remove_event_handler(event_handler &handler)
{
	scoped_lock lock(mutex_);
	handlers_.erase(std::remove(handlers_.begin(), handlers_.end(), &handler), handlers_.end());
}


bool autobanner::is_banned(std::string_view address, address_type type)
{
	if (opts_.max_login_failures() == 0)
		return false;

	scoped_lock lock(mutex_);

	auto impl = [this](const auto &map, auto ip) {
		if (auto it = map.find(ip); it != map.end())
			return it->second.failed_timepoints.size() >= opts_.max_login_failures();

		return false;
	};

	if (type == address_type::ipv4) {
		util::parseable_range r(address);

		if (hostaddress::ipv4_host h; parse_ip(r, h) && eol(r))
			return impl(ipv4_map_, h.to_uint32());
	}
	else
	if (type == address_type::ipv6) {
		util::parseable_range r(address);

		if (hostaddress::ipv6_host h; parse_ip(r, h) && eol(r))
			return impl(ipv6_map_, h.high_to_uint64());
	}

	return true;
}

bool autobanner::set_failed_login(std::string_view address, address_type type)
{
	if (opts_.max_login_failures() == 0)
		return false;

	scoped_lock lock(mutex_);

	auto add_timer = [&](auto &map, auto ip, auto &handle, duration d) {
		handle.timer_id = this->add_timer(d, true);
		map.emplace(std::piecewise_construct,
			std::forward_as_tuple(handle.timer_id),
			std::forward_as_tuple(ip)
		);
	};

	auto impl = [&](auto &map, auto &timer_map, auto ip) {
		auto &handle = map[ip];

		if (handle.failed_timepoints.size() >= opts_.max_login_failures())
			return true;

		stop_timer(handle.timer_id);

		handle.failed_timepoints.push_back(monotonic_clock::now());

		if (handle.failed_timepoints.size() >= opts_.max_login_failures()) {
			if (handle.failed_timepoints.back() - handle.failed_timepoints.front() <= opts_.login_failures_time_window()) {
				add_timer(timer_map, ip, handle, opts_.ban_duration());

				for (auto eh: handlers_)
					eh->send_event<banned_event>(address, type);

				return true;
			}

			handle.failed_timepoints.pop_front();
		}

		add_timer(timer_map, ip, handle, opts_.login_failures_time_window());
		return false;
	};


	if (type == address_type::ipv4) {
		util::parseable_range r(address);

		if (hostaddress::ipv4_host h; parse_ip(r, h) && eol(r))
			return impl(ipv4_map_, ipv4_timer_map_, h.to_uint32());
	}
	else
	if (type == address_type::ipv6) {
		util::parseable_range r(address);

		if (hostaddress::ipv6_host h; parse_ip(r, h) && eol(r))
			return impl(ipv6_map_, ipv6_timer_map_, h.high_to_uint64());
	}

	return true;
}

void autobanner::operator()(const event_base &ev)
{
	fz::dispatch<timer_event>(ev, [this](timer_id id) {
		scoped_lock lock(mutex_);

		if (auto it = ipv4_timer_map_.find(id); it != ipv4_timer_map_.end()) {
			ipv4_map_.erase(it->second);
			ipv4_timer_map_.erase(it);
		}
		else
		if (auto it = ipv6_timer_map_.find(id); it != ipv6_timer_map_.end()) {
			ipv6_map_.erase(it->second);
			ipv6_timer_map_.erase(it);
		}
	});
}

}
