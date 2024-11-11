#include <libfilezilla/process.hpp>

#include "../receiver/async.hpp"
#include "../util/bits.hpp"
#include "../logger/type.hpp"

#include "checker.hpp"

#include "../serialization/archives/xml.hpp"
#include "../serialization/types/update.hpp"
#include "../serialization/types/time.hpp"

namespace fz::update {

checker::checker(event_loop &loop, info::retriever &retriever, enabled_for_receiving_base &receiver, util::fs::native_path cache, logger_interface &logger, options opts)
	: util::invoker_handler(loop)
	, retriever_(retriever)
	, receiver_(receiver)
	, cache_(std::move(cache))
	, logger_(logger, "Update Checker")
	, timer_id_{}
	, is_checking_now_{}
{
	load_cache();
	set_options(std::move(opts));
}

checker::~checker()
{
	remove_handler_and_stop_receiving();
}

void checker::load_cache()
{
	using namespace serialization;

	xml_input_archive::file_loader l(cache_);
	xml_input_archive ar(l, xml_input_archive::options().verify_version(xml_input_archive::options::verify_mode::error));

	ar(
		nvp(last_info_, "last_info"),
		nvp(last_info_dt_, "last_info_dt"),
		nvp(last_check_dt_, "last_check_dt"),
		nvp(error_count_, "error_count")
	);

	if (!ar) {
		last_info_ = {};
		last_info_dt_ = {};
		error_count_ = {};
		last_check_dt_ = {};
	}
	else
	if (last_info_ && !last_info_.is_allowed(opts_.allowed_type())) {
		last_info_ = {};
		last_info_dt_ = {};
	}
	else {
		logger_.log(logmsg::debug_info, L"Loaded cache from file `%s'.", cache_.str());
	}
}

bool checker::save_cache()
{
	using namespace serialization;

	bool result = false;

	{
		xml_output_archive::file_saver l(cache_);
		xml_output_archive ar(l, xml_output_archive::options().save_result(&result));

		ar(
			nvp(last_info_, "last_info"),
			nvp(last_info_dt_, "last_info_dt"),
			nvp(last_check_dt_, "last_check_dt"),
			nvp(error_count_, "error_count")
		);
	}

	return result;
}

void checker::set_options(options opts)
{
	if (opts.frequency() < fz::duration::from_seconds(0)) {
		logger_.log(logmsg::error, L"Frequency was negative. This is not allowed, disabling updates.");
		opts.frequency() = fz::duration::from_seconds(0);
	}

	scoped_lock lock(mutex_);
	opts_ = std::move(opts);

	auto old_next_check_dt = next_check_dt_;
	auto old_last_info = last_info_;

	reschedule();

	if (old_next_check_dt != next_check_dt_ || old_last_info != last_info_)
		receiver_handle<result>{receiver_}(last_info_, last_check_dt_, next_check_dt_);
}

checker::options checker::get_options()
{
	scoped_lock lock(mutex_);
	return opts_;
}

void checker::start()
{
	scoped_lock lock(mutex_);

	if (!started_) {
		started_ = true;
		reschedule();
	}
}

void checker::stop()
{
	scoped_lock lock(mutex_);

	if (started_) {
		started_ = false;
		reschedule();
	}
}

bool checker::check_now()
{
	scoped_lock lock(mutex_);

	return do_check_now(true);
}

bool checker::is_checking_now() const
{
	scoped_lock lock(mutex_);

	return is_checking_now_;
}

info checker::get_last_checked_info() const
{
	scoped_lock lock(mutex_);

	return last_info_;
}

datetime checker::get_last_check_dt() const
{
	scoped_lock lock(mutex_);

	return last_check_dt_;
}

datetime checker::get_next_check_dt() const
{
	scoped_lock lock(mutex_);

	return next_check_dt_;
}

auto dt2ms(const fz::datetime &dt)
{
	return (dt-fz::datetime(0, datetime::milliseconds)).get_milliseconds();
}

void checker::reschedule()
{
	datetime next_check;

	// EOL disables automatic checking
	if (opts_.frequency() && !last_info_.is_eol() && started_) {
		if (error_count_) {
			logger_.log(logmsg::debug_warning, L"There have been %d consecutive errors so far. Using exponential backoff for the next check time.", error_count_);
			logger_.log(logmsg::debug_info, L"Last check was at: %s", last_check_dt_.format("%c", fz::datetime::zone::local));

			next_check = last_check_dt_ + duration::from_minutes(std::min(
				util::exp2_saturated<std::int64_t>(error_count_-1),
				opts_.frequency().get_minutes()
			));
		}
		else
		if (last_info_dt_) {
			next_check = last_info_dt_ + opts_.frequency();
		}

		if (next_check.earlier_than(datetime::now())) {
			error_count_ = 0;
			next_check = datetime::now();
		}
	}

	check_at(next_check);
}

/****************************************/

bool checker::do_check_now(bool manual)
{
	if (is_checking_now_) {
		logger_.log_raw(logmsg::debug_info, L"Already checking, nothing more to do.");
		return false;
	}

	is_checking_now_ = true;

	last_check_dt_ = datetime::now();

	retrieve_info(manual, async_receive(this) >> [&](auto &expected_info) {
		scoped_lock lock(mutex_);

		if (expected_info) {
			last_info_ = *expected_info;
			last_info_dt_ = last_check_dt_;
			error_count_ = 0;

			if (!opts_.callback_path().empty() && last_info_.is_newer_than(fz::build_info::version)) {
				logger_.log_u(logmsg::status, L"Running program '%s'", opts_.callback_path());
				fz::spawn_detached_process({opts_.callback_path(), fz::native_string(last_info_->version)});
			}
		}
		else {
			logger_.log(logmsg::error, L"Got error from retriever: %s", expected_info.error());

			// Do not wrap around
			if (error_count_ < std::size_t(-1)) {
				error_count_ += 1;
			}
		}

		save_cache();
		reschedule();

		receiver_handle<result>{receiver_}(std::move(expected_info), last_check_dt_, next_check_dt_);

		is_checking_now_ = false;
	});

	return true;
}

void checker::retrieve_info(bool manual, receiver_handle<info::retriever::result> h)
{
	auto info = get_last_checked_info();

	// Skip if doing a manual check
	if (manual); else
	// Skip if there's no cached info.
	if (!info); else
	// Then, skip if the cached info represent an update that is not allowed at the moment
	if (!info.is_allowed(opts_.allowed_type())); else
	// Skip also if the cached info has a version which isn't newer than our own
	if (!info.is_newer_than(fz::build_info::version)); else
	// And, finally, skip if the timestamp of the cache is too old
	if ((fz::datetime::now() - last_info_dt_) >= get_options().frequency());
	// Otherwise, return the cached info as is.
	else {
		logger_.log_u(fz::logmsg::debug_info, L"Returning info from internal cache.");
		return h(std::move(info));
	}

	if (!save_cache()) {
		// Do not perform any actual check if the cache cannot be saved.
		return h(fz::unexpected("Couldn't save info to cache"));
	}

	// Foward to the actual retriever
	return retriever_.retrieve_info(manual, opts_.allowed_type(), std::move(h));
}

void checker::check_at(datetime dt)
{
	if (dt) {
		if (dt != next_check_dt_) {
			logger_.log(logmsg::status, L"The next check will be performed at: %s.", dt.format("%c", fz::datetime::zone::local));
		}

		auto delta = dt - datetime::now();

		if (delta > duration::from_days(1))
			delta = duration::from_days(1);
		else
		if (delta > duration::from_minutes(1))
			delta = duration::from_minutes(1);

		timer_id_ = stop_add_timer(timer_id_, delta, true);
	}
	else
	if (timer_id_) {
		logger_.log_raw(logmsg::status, L"Automatic check has been disabled.");

		stop_timer(timer_id_);
		timer_id_ = {};
	}

	next_check_dt_ = dt;
}

void checker::operator()(const event_base &ev)
{
	if (on_invoker_event(ev))
		return;

	fz::dispatch<timer_event>(ev, [&](timer_id) {
		scoped_lock lock(mutex_);

		timer_id_ = {};

		if (next_check_dt_ > datetime::now())
			return check_at(next_check_dt_);

		do_check_now(false);
	});
}

}
