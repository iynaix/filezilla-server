#ifndef FZ_AUTHENTICATION_AUTOBANNER_HPP
#define FZ_AUTHENTICATION_AUTOBANNER_HPP

#include <cstdint>
#include <unordered_map>
#include <deque>
#include <string_view>

#include <libfilezilla/iputils.hpp>
#include <libfilezilla/event_handler.hpp>

#include "../util/options.hpp"

namespace fz::authentication {

class autobanner: private event_handler
{
public:
	struct options: util::options<options, autobanner>
	{
		opt<uint16_t> max_login_failures         = o(0);
		opt<duration> login_failures_time_window = o(duration::from_milliseconds(100));
		opt<duration> ban_duration               = o(duration::from_minutes(5));

		options() {}
	};

	using banned_event = simple_event<autobanner, std::string /*address*/, address_type /* type */>;

	autobanner(event_loop &loop, options opts = {});
	~autobanner() override;

	void set_options(options opts);

	bool is_banned(std::string_view address, address_type type);
	bool set_failed_login(std::string_view address, address_type type);

	class with_events;

private:
	friend with_events;

	void add_event_handler(event_handler &target_handler);
	void remove_event_handler(event_handler &target_handler);

	void operator()(const event_base &ev) override;

	struct handle
	{
		std::deque<monotonic_clock> failed_timepoints;
		fz::timer_id timer_id{};
	};


	fz::mutex mutex_;

	options opts_;

	std::vector<event_handler *> handlers_{};

	std::unordered_map<std::uint32_t, handle> ipv4_map_;
	std::unordered_map<std::uint64_t, handle> ipv6_map_;
	std::unordered_map<timer_id, std::uint32_t> ipv4_timer_map_;
	std::unordered_map<timer_id, std::uint64_t> ipv6_timer_map_;
};

class autobanner::with_events
{
public:
	with_events(autobanner &ref, event_handler &eh)
		: ref_(ref)
		, eh_(eh)
	{
		ref_.add_event_handler(eh_);
	}

	~with_events()
	{
		ref_.remove_event_handler(eh_);
	}

	operator autobanner &()
	{
		return ref_;
	}

	autobanner *operator &()
	{
		return &ref_;
	}

	void set_options(options opts)
	{
		ref_.set_options(std::move(opts));
	}

	bool is_banned(std::string_view address, address_type type)
	{
		return ref_.is_banned(std::move(address), type);
	}

	bool set_failed_login(std::string_view address, address_type type)
	{
		return ref_.set_failed_login(std::move(address), type);
	}

private:
	autobanner &ref_;
	event_handler &eh_;
};

}

#endif // FZ_AUTHENTICATION_AUTOBANNER_HPP
