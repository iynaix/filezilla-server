#include <cassert>

#include "throttled_authenticator.hpp"
#include "../remove_event.hpp"
#include "../hostaddress.hpp"

namespace fz::authentication {

class throttled_authenticator::worker: private event_handler {
	class operation;

public:
	worker(throttled_authenticator &owner, std::string_view name, address_type family, std::string_view ip, event_handler *target, logger::modularized::meta_map meta_for_logging)
		: event_handler{owner.event_loop_}
		, name_(name)
		, family_(family)
		, ip_(ip)
		, target_(target)
		, owner_(owner)
		, meta_for_logging_(std::move(meta_for_logging))
		, logger_(owner.logger_, {}, meta_for_logging_)
	{
	}

	bool try_to_authenticate(const methods_list &methods, std::unique_ptr<authenticator::operation> next_op = {});
	bool authenticate();

	void record_failure();

	~worker() override {
		remove_handler();
		owner_.wrapped_.stop_ongoing_authentications(*this);

		if (record_failure_about_failed_none_auth_on_destruction_) {
			logger_.log(logmsg::debug_info, L"No other auth method was attempted, thus record the 'none' authentication failure.");
			record_failure();
		}
		else
		if (authenticating_) {
			logger_.log(logmsg::debug_info, L"Destroying while auth still in progress: recording failure to mitigate a possible DoS.");
			record_failure();
		}

		if (self_in_waiting_)
			owner_.waiting_workers_.erase(*self_in_waiting_);
	}

	void remove()
	{
		fz::scoped_lock lock(owner_.mutex_);

		owner_.workers_.erase(self_in_workers_);
	}

private:
	friend throttled_authenticator;

	void operator()(const event_base &ev) override;
	void on_operation_result(authenticator &, std::unique_ptr<authenticator::operation> &op);

	std::string name_{};
	methods_list methods_{};
	address_type family_{};
	std::string ip_{};
	event_handler *target_{};
	throttled_authenticator &owner_;
	logger::modularized::meta_map meta_for_logging_;
	logger::modularized logger_;

	std::size_t number_of_failed_none_auths_to_report_{};
	bool record_failure_about_failed_none_auth_on_destruction_{};
	bool authenticating_{};
	std::unique_ptr<authenticator::operation> next_op_{};

	workers_t::iterator self_in_workers_;
	std::optional<waiting_workers_t::iterator> self_in_waiting_;
};

class throttled_authenticator::worker::operation: public authenticator::operation {
public:
	shared_user get_user() override;
	virtual available_methods get_methods() override;
	virtual error get_error() override;

	virtual bool next(const methods_list &methods) override;
	void stop() override;

	operation(worker &w, std::unique_ptr<authenticator::operation> &&wrapped);

private:
	worker *worker_;
	std::unique_ptr<authenticator::operation> wrapped_;
};

shared_user throttled_authenticator::worker::operation::get_user()
{
	return wrapped_->get_user();
}

available_methods throttled_authenticator::worker::operation::get_methods()
{
	return wrapped_->get_methods();
}

error throttled_authenticator::worker::operation::get_error()
{
	return wrapped_->get_error();
}

bool throttled_authenticator::worker::operation::next(const methods_list &methods)
{
	bool res = false;

	if (worker_) {
		fz::scoped_lock lock(worker_->owner_.mutex_);
		res = worker_->try_to_authenticate(methods, std::move(wrapped_));

		worker_ = nullptr;
	}

	return res;
}

void throttled_authenticator::worker::operation::stop()
{
	authentication::stop(std::move(wrapped_));

	if (worker_) {
		fz::scoped_lock lock(worker_->owner_.mutex_);

		worker_->authenticating_ = false;
		worker_->logger_.log_u(logmsg::debug_debug, L"operation %p stop() erasing worker %p", this, worker_);
		worker_->remove();
		worker_ = nullptr;
	}
}

throttled_authenticator::worker::operation::operation(worker &w, std::unique_ptr<authenticator::operation> &&wrapped)
	: worker_(&w)
	, wrapped_(std::move(wrapped))
{
	w.logger_.log_u(logmsg::debug_debug, L"Worker %p created new operation %p", &w.owner_, this);
}


bool throttled_authenticator::failures_t::add(duration delay, duration cap, std::size_t max_failures, duration failures_window)
{
	auto now = monotonic_clock::now();

	if (max_failures > 0) {
		purge_old(now, failures_window);

		if (timepoints_.size() < max_failures)
			timepoints_.push_back(now);
		else
			timepoints_.back() = now;
	}

	return set_next_try(now, delay, cap, max_failures);
}

bool throttled_authenticator::failures_t::purge_old(monotonic_clock now, duration failures_window)
{
	if (timepoints_.size()) {
		auto oldest_useful_timepoint = (monotonic_clock(now) -= failures_window);

		do {
			if (oldest_useful_timepoint < timepoints_.front())
				break;

			timepoints_.pop_front();
		} while (timepoints_.size());
	}

	return timepoints_.empty();
}

const monotonic_clock &throttled_authenticator::failures_t::next_try() const
{
	return next_try_;
}

bool throttled_authenticator::failures_t::set_next_try(monotonic_clock now, duration delay, duration cap, std::size_t max_failures)
{
	auto must_be_delayed = timepoints_.size() >= max_failures;

	if (must_be_delayed)
		next_try_ = std::min(std::max(next_try_, now) + delay, now + cap);
	else
		next_try_ = now;

	return must_be_delayed;
}

void throttled_authenticator::worker::operator()(const event_base &ev)
{
	dispatch<
		operation::result_event
	>(ev, this,
		&worker::on_operation_result
	);
}

void throttled_authenticator::worker::on_operation_result(authenticator &, std::unique_ptr<authenticator::operation> &op)
{
	fz::scoped_lock lock(owner_.mutex_);

	authenticating_ = false;

	bool is_none = methods_.size() == 1 && methods_[0].is<method::none>();

	if (!is_none && record_failure_about_failed_none_auth_on_destruction_) {
		logger_.log(logmsg::debug_info, L"A 'none' auth previously failed, but a new auth has been attempted, so no need to record the failure on destruction.");
		record_failure_about_failed_none_auth_on_destruction_ = false;
	}

	if (op->get_error() && op->get_error() != error::internal && !methods_.just_verify()) {
		if (is_none) {
			if (number_of_failed_none_auths_to_report_ == 0) {
				logger_.log(logmsg::debug_info, L"First 'none' authentication attempt failed. Recording the event for later just in case.");
				record_failure_about_failed_none_auth_on_destruction_ = true;
			}
			else
			if (number_of_failed_none_auths_to_report_ == 1) {
				logger_.log(logmsg::debug_info, L"Second 'none' authentication attempt failed. Recording previous failure.");
				record_failure();
				record_failure_about_failed_none_auth_on_destruction_ = false;
			}

			number_of_failed_none_auths_to_report_ += 1;
		}

		if (!is_none || number_of_failed_none_auths_to_report_ > 1) {
			record_failure();
		}
	}

	target_->send_event<operation::result_event>(owner_, std::make_unique<operation>(*this, std::move(op)));
}

bool throttled_authenticator::worker::try_to_authenticate(const methods_list &methods, std::unique_ptr<authenticator::operation> next_op)
{
	methods_ = methods;
	next_op_ = std::move(next_op);

	auto now = monotonic_clock::now();
	auto next_try = now;

	auto update_next_try = [&next_try, &now, this](auto &map, const auto &key) {
		if (auto it = map.find(key); it != map.end()) {
			if (next_try < it->second.next_try())
				next_try = it->second.next_try();

			if (now < it->second.next_try())
				it->second.set_next_try(now, owner_.opts_.delay(), owner_.opts_.cap(), owner_.opts_.max_failures());
		}
	};

	if (family_ == address_type::ipv4) {
		util::parseable_range r(ip_);

		if (hostaddress::ipv4_host h; parse_ip(r, h) && eol(r))
			update_next_try(owner_.ipv4_failures_, h.to_uint32());
	}
	else
	if (family_ == address_type::ipv6) {
		util::parseable_range r(ip_);

		if (hostaddress::ipv6_host h; parse_ip(r, h) && eol(r))
			update_next_try(owner_.ipv6_failures_, h.high_to_uint64());
	}

	update_next_try(owner_.users_failures_, name_);

	if (next_try == now)
		return authenticate();

	auto delta = next_try - now;

	logger_.log_u(logmsg::debug_warning, L"Authentication for user %s from IP %s will be delayed %ds.", name_, ip_, delta.get_seconds());
	self_in_waiting_ = owner_.waiting_workers_.emplace(next_try, self_in_workers_);

	if (owner_.auth_timer_id_ == 0)
		owner_.auth_timer_id_ = owner_.add_timer(delta, true);

	return true;
}

bool throttled_authenticator::worker::authenticate()
{
	if (next_op_) {
		if (!authentication::next(std::move(next_op_), methods_))
			return false;
	}
	else {
		logger_.log_u(logmsg::debug_info, L"Authenticating user %s from IP %s.", name_, ip_);
		owner_.wrapped_.authenticate(name_, methods_, family_, ip_, *this, std::move(meta_for_logging_));
	}

	return authenticating_ = true;
}

void throttled_authenticator::worker::record_failure()
{
	logger_.log_u(logmsg::debug_info, L"Recording failed login for user %s from IP %s.", name_, ip_);

	auto add_failure = [&](auto &map, const auto &key) -> duration {
		auto &f = map[key];
		bool must_be_delayed = f.add(owner_.opts_.delay(), owner_.opts_.cap(), owner_.opts_.max_failures(), owner_.opts_.failures_window());

		if (must_be_delayed)
			return f.next_try() - monotonic_clock::now();

		return {};
	};

	if (auto delta = add_failure(owner_.users_failures_, name_))
		logger_.log_u(logmsg::debug_warning, L"User %s has failed login too many times (>= %d) within a %ds time window. Next login will be delayed %ds from now.",
							 name_, owner_.opts_.max_failures(), owner_.opts_.failures_window().get_seconds(), delta.get_seconds());

	if (family_ == address_type::ipv4) {
		util::parseable_range r(ip_);

		if (hostaddress::ipv4_host h; parse_ip(r, h) && eol(r)) {
			if (auto delta = add_failure(owner_.ipv4_failures_, h.to_uint32()))
				logger_.log_u(logmsg::debug_warning, L"Login from IP %s has failed too many times (>= %d) within a %ds time window. Next login will be delayed %ds from now.",
									 ip_, owner_.opts_.max_failures(), owner_.opts_.failures_window().get_seconds(), delta.get_seconds());
		}
	}
	else
	if (family_ == address_type::ipv6) {
		util::parseable_range r(ip_);

		if (hostaddress::ipv6_host h; parse_ip(r, h) && eol(r)) {
			if (auto delta = add_failure(owner_.ipv6_failures_, h.high_to_uint64()))
				logger_.log_u(logmsg::debug_warning, L"Login from IP %s has failed too many times (>= %d) within a %ds time window. Next login will be delayed %ds from now.",
									 ip_, owner_.opts_.max_failures(), owner_.opts_.failures_window().get_seconds(), delta.get_seconds());
		}
	}
	else {
		logger_.log_u(logmsg::error, L"Internal error: wrong IP family type %d.", family_);
	}

	if (owner_.purging_timer_id_ == 0)
		owner_.purging_timer_id_ = owner_.add_timer(owner_.opts_.failures_window(), true);
}

throttled_authenticator::throttled_authenticator(fz::event_loop &loop, fz::authentication::authenticator &wrapped, logger_interface &logger, options opts)
	: event_handler(loop)
	, wrapped_(wrapped)
	, logger_(logger, "Throttled Authenticator")
{
	set_options(std::move(opts));
}

throttled_authenticator::~throttled_authenticator()
{
	remove_handler();

	workers_.clear();
}

void throttled_authenticator::authenticate(std::string_view name, const methods_list &methods, address_type family, std::string_view ip, fz::event_handler &target, logger::modularized::meta_map meta_for_logging)
{
	fz::scoped_lock lock(mutex_);

	auto &worker = workers_.emplace_front(*this, name, family, ip, &target, std::move(meta_for_logging));
	worker.self_in_workers_ = workers_.begin();

	worker.try_to_authenticate(methods);
}

void throttled_authenticator::stop_ongoing_authentications(event_handler &target)
{
	fz::scoped_lock lock(mutex_);

	remove_events<operation::result_event>(&target, *this);

	workers_.remove_if([&](worker &w) {
		return w.target_ == &target;
	});
}

void throttled_authenticator::set_options(throttled_authenticator::options opts)
{
	fz::scoped_lock lock(mutex_);

	opts_ = std::move(opts);
}

void throttled_authenticator::operator()(const event_base &ev)
{
	fz::dispatch<timer_event>(ev, [&](timer_id id) {
		auto now = fz::monotonic_clock::now();

		fz::scoped_lock lock(mutex_);

		if (id == auth_timer_id_) {
			auth_timer_id_ = 0;

			logger_.log_u(logmsg::debug_debug, L"Number of waiting auths before cycle: %d.", waiting_workers_.size());
			while (!waiting_workers_.empty()) {
				auto it = waiting_workers_.begin();

				if (now < it->first) {
					auto delta = it->first - now;
					logger_.log_u(logmsg::debug_debug, L"Next auth for user %s from ip %s will be attempted in %ds.", it->second->name_, it->second->ip_, delta.get_seconds());
					auth_timer_id_ = add_timer(delta, true);

					break;
				}

				it->second->self_in_waiting_ = {};
				it->second->authenticate();
				waiting_workers_.erase(it);
			}
			logger_.log_u(logmsg::debug_debug, L"Number of waiting auths after cycle: %d.", waiting_workers_.size());
		}
		else
		if (id == purging_timer_id_) {
			purging_timer_id_ = 0;

			auto purge = [&](auto &map, const wchar_t *name) {
				logger_.log_u(logmsg::debug_debug, L"Number of %s failures before purging cycle: %d.", name, map.size());

				for (auto it = map.begin(); it != map.end();) {
					auto next = std::next(it);

					if (it->second.purge_old(now, opts_.failures_window()))
						map.erase(it);

					it = next;
				}
				logger_.log_u(logmsg::debug_debug, L"Number of %s failures after purging cycle: %d.", name, map.size());
			};

			purge(ipv4_failures_, L"IPv4");
			purge(ipv6_failures_, L"IPv6");
			purge(users_failures_, L"users");

			if (!users_failures_.empty() || !ipv4_failures_.empty() || !ipv6_failures_.empty())
				purging_timer_id_ = add_timer(opts_.failures_window(), true);
		}
	});
}

}
