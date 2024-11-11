#include "notifier.hpp"
#include "debug.hpp"
#include "../../filezilla/mpl/for_each.hpp"

administrator::notifier::notifier(administrator &administrator, fz::tcp::session::id id, const fz::datetime &start, const std::string &peer_ip, fz::address_type peer_address_type, fz::logger_interface &logger)
	: administrator_(&administrator)
	, session_id_(id)
	, start_(start)
	, monotonic_start_(fz::monotonic_clock::now())
	, peer_ip_(peer_ip)
	, peer_address_type_(peer_address_type)
	, log_forwarder_(logger, administrator, id)
{
	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	if (num_of_sessions > 0)
		administrator_->admin_server_.broadcast<administration::session::start>(session_id_, start_, peer_ip_, peer_address_type_);
}

administrator::notifier::~notifier()
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	if (num_of_sessions > 0)
		administrator_->admin_server_.broadcast<administration::session::stop>(session_id_, fz::monotonic_clock::now()-monotonic_start_);
}

void administrator::notifier::notify_user_name(std::string_view name)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	user_name_ = name;
	user_name_set_time_ = fz::monotonic_clock::now()-monotonic_start_;

	if (num_of_sessions > 0)
		administrator_->admin_server_.broadcast<administration::session::user_name>(session_id_, user_name_set_time_, name);
}

void administrator::notifier::notify_entry_open(std::uint64_t id, std::string_view path, int64_t size)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	auto &e = entries_[id];
	e.path = path;
	e.size = size;
	e.open_time_ = fz::monotonic_clock::now()-monotonic_start_;

	if (num_of_sessions > 0)
		administrator_->admin_server_.broadcast<administration::session::entry_open>(session_id_, e.open_time_, id, path, size);
}

void administrator::notifier::notify_entry_close(std::uint64_t id, int error)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	entries_.erase(id);

	if (num_of_sessions > 0) {
		auto now = fz::monotonic_clock::now()-monotonic_start_;
		administrator_->admin_server_.broadcast<administration::session::entry_close>(session_id_, now, id, error);
	}
}

void administrator::notifier::notify_entry_write(std::uint64_t id, std::int64_t amount, std::int64_t offset)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	if (amount < 0)
		return;

	auto it = entries_.find(id);
	if (it == entries_.end())
		return;

	auto &e = it->second;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	auto now = fz::monotonic_clock::now()-monotonic_start_;

	if (e.size >= 0) {
		if (offset < 0)
			offset = e.size;

		e.size = std::max(e.size, offset + amount);
	}

	e.last_written_time_ = now;
	e.written += amount;

	if (num_of_sessions > 0) {
		bool must_report = !e.last_reported_written_time_ || (now - e.last_reported_written_time_).get_milliseconds() >= 200;

		if (must_report) {
			e.last_reported_written_time_ = now;
			administrator_->admin_server_.broadcast<administration::session::entry_written>(session_id_, now, id, e.written, e.size);
		}
	}
}

void administrator::notifier::notify_entry_read(std::uint64_t id, std::int64_t amount, std::int64_t)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	if (amount < 0)
		return;

	auto it = entries_.find(id);
	if (it == entries_.end())
		return;

	auto &e = it->second;

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	auto now = fz::monotonic_clock::now()-monotonic_start_;

	e.last_read_time_ = now;
	e.read += amount;

	if (num_of_sessions > 0) {
		bool must_report = !e.last_reported_read_time_ || (now - e.last_reported_read_time_).get_milliseconds() >= 200;

		if (must_report) {
			e.last_reported_read_time_ = now;
			administrator_->admin_server_.broadcast<administration::session::entry_read>(session_id_, now, id, e.read);
		}
	}
}

void administrator::notifier::notify_protocol_info(const protocol_info &info)
{
	fz::scoped_lock lock(mutex_);

	if (!administrator_)
		return;

	fz::mpl::for_each<administration::session::any_protocol_info::variant>([&](auto i) {
		using T = typename decltype(i)::type;

		if (auto v = dynamic_cast<const T*>(&info)) {
			proto_info_set_time_ = fz::monotonic_clock::now()-monotonic_start_;
			proto_info_ = *v;

			return false;
		}

		return true;
	});

	auto num_of_sessions = administrator_->admin_server_.get_number_of_sessions();

	ADMINISTRATOR_DEBUG_LOG(L"%s - ns: %d", __PRETTY_FUNCTION__, num_of_sessions);

	if (num_of_sessions > 0 && proto_info_)
		administrator_->admin_server_.broadcast<administration::session::protocol_info>(session_id_, proto_info_set_time_, *proto_info_);
}


fz::logger_interface &administrator::notifier::logger()
{
	return log_forwarder_;
}

void administrator::notifier::send_session_info(administration::engine::session &session) const
{
	fz::scoped_lock lock(mutex_);

	if (administrator_->admin_server_.get_number_of_sessions() == 0)
		return;

	session.send<administration::session::start>(session_id_, start_, peer_ip_, peer_address_type_);
	session.send<administration::session::user_name>(session_id_, user_name_set_time_, user_name_);

	if (proto_info_)
		session.send<administration::session::protocol_info>(session_id_, proto_info_set_time_,*proto_info_);

	for (auto &[id, e]: entries_) {
		session.send<administration::session::entry_open>(session_id_, e.open_time_, id, e.path, e.size);

		if (e.last_written_time_)
			session.send<administration::session::entry_written>(session_id_, e.last_written_time_, id, e.written, e.size);

		if (e.last_read_time_)
			session.send<administration::session::entry_read>(session_id_, e.last_read_time_, id, e.read);
	}
}

void administrator::notifier::detach_from_administrator()
{
	fz::scoped_lock lock(mutex_);

	administrator_ = nullptr;

	log_forwarder_.detach_from_administrator();
}

administrator::log_forwarder::log_forwarder(administrator &administrator, fz::tcp::session::id session_id)
	: administrator_(&administrator)
	, session_id_(session_id)
{
}

administrator::log_forwarder::log_forwarder(fz::logger_interface &parent, administrator &administrator, fz::tcp::session::id session_id)
	: modularized(parent)
	, administrator_(&administrator)
	, session_id_(session_id)
{
}

void administrator::log_forwarder::do_log(fz::logmsg::type t, const info_list &l, std::wstring &&message)
{
	{
		fz::scoped_lock lock(mutex_);

		if (administrator_ && administrator_->admin_server_.get_number_of_sessions() > 0)
			administrator_->admin_server_.broadcast<administration::log>(fz::datetime::now(), session_id_, t, l.as_string, message);
	}

	modularized::do_log(t, l, std::move(message));
}

void administrator::log_forwarder::detach_from_administrator()
{
	fz::scoped_lock lock(mutex_);

	administrator_ = nullptr;
}
