#ifndef FZ_AUTHENTICATION_THROTTLED_AUTHENTICATOR_HPP
#define FZ_AUTHENTICATION_THROTTLED_AUTHENTICATOR_HPP

#include <list>
#include <unordered_map>
#include <map>
#include <set>
#include <deque>

#include "../filezilla/logger/modularized.hpp"
#include "../filezilla/util/options.hpp"

#include "authenticator.hpp"

namespace fz::authentication
{

class throttled_authenticator: private event_handler, public authenticator
{
public:
	struct options: util::options<options, throttled_authenticator>
	{
		opt<std::size_t> max_failures    = o(5);
		opt<duration>    failures_window = o(duration::from_seconds(60));
		opt<duration>    delay           = o(duration::from_seconds(5));
		opt<duration>    cap             = o(duration::from_seconds(60));

		options(){}
	};

	throttled_authenticator(event_loop &loop, authenticator &wrapped, logger_interface &logger, options opts = {});
	~throttled_authenticator() override;

	void authenticate(std::string_view name, const methods_list &methods, address_type family, std::string_view ip, event_handler &target, logger::modularized::meta_map meta_for_logging = {}) override;
	void stop_ongoing_authentications(event_handler &target) override;

	void set_options(options opts);

private:
	class worker;

	authenticator &wrapped_;
	logger::modularized logger_;

	mutable mutex mutex_;

	using workers_t = std::list<worker>;
	workers_t workers_;

private:
	void operator()(const event_base &ev) override;

	fz::timer_id auth_timer_id_{};
	fz::timer_id purging_timer_id_{};

private:
	class failures_t
	{
	public:
		bool add(duration delay, duration cap, std::size_t max_failures, duration failures_window);
		bool purge_old(monotonic_clock now, duration failures_window);
		bool set_next_try(monotonic_clock now, duration delay, duration cap, std::size_t max_failures);
		const monotonic_clock &next_try() const;

	private:
		std::deque<monotonic_clock> timepoints_;
		fz::monotonic_clock next_try_;
	};

	using waiting_workers_t = std::multimap<fz::monotonic_clock, workers_t::iterator>;
	using ipv4_failures_t = std::unordered_map<std::uint32_t, failures_t>;
	using ipv6_failures_t = std::unordered_map<std::uint64_t, failures_t>;
	using users_failures_t = users_map<failures_t>;

	waiting_workers_t waiting_workers_;
	ipv4_failures_t ipv4_failures_;
	ipv6_failures_t ipv6_failures_;
	users_failures_t users_failures_;

	options opts_;
};

}

#endif // FZ_AUTHENTICATION_THROTTLED_AUTHENTICATOR_HPP
