#include "listener.hpp"
#include "hostaddress.hpp"
#include "../remove_event.hpp"
#include "../util/parser.hpp"
#include "../logger/type.hpp"

fz::tcp::listener::listener(fz::thread_pool &pool,
							event_loop &loop,
							event_handler &target_handler,
							logger_interface &logger,
							address_info address_info,
							const peer_allowance_checker &pac,
							status_change_notifier &scn,
							user_data user_data)
	: event_handler{loop}
	, pool_{pool}
	, target_handler_{target_handler}
	, logger_{logger}
	, address_info_(address_info)
	, peer_allowance_checker_(pac)
	, status_change_notifier_(scn)
	, user_data_(std::move(user_data))
{
}

fz::tcp::listener::~listener() {
	stop();
	remove_handler();
}

const fz::tcp::address_info &fz::tcp::listener::get_address_info() const
{
	return address_info_;
}

fz::tcp::address_info &fz::tcp::listener::get_address_info()
{
	return address_info_;
}


const fz::tcp::listener::user_data &fz::tcp::listener::get_user_data() const
{
	return user_data_;
}

fz::tcp::listener::user_data &fz::tcp::listener::get_user_data()
{
	return user_data_;
}

fz::tcp::listener::status fz::tcp::listener::get_status() const
{
	fz::scoped_lock lock(mutex_);

	return status_;
}

std::unique_ptr<fz::socket> fz::tcp::listener::get_socket()
{
	fz::scoped_lock lock(mutex_);

	std::unique_ptr<fz::socket> ret;

	if (!accepted_.empty()) {
		ret = std::move(accepted_.front());
		accepted_.pop_front();
	}

	return ret;
}

bool fz::tcp::listener::has_socket()
{
	fz::scoped_lock lock(mutex_);

	return !accepted_.empty();
}

void fz::tcp::listener::start()
{
	fz::scoped_lock lock(mutex_);

	if (!listen_socket_) {
		socket_base::socket_t fd = -1;
		util::parseable_range r(address_info_.address);

		if (match_string(r, "file_descriptor:") && parse_int(r, fd) && eol(r)) {
			int error = 0;
			listen_socket_ = listen_socket::from_descriptor(socket_descriptor(fd), pool_, error, static_cast<event_handler *>(const_cast<listener *>(this)));
			if (listen_socket_) {
				auto status_log = user_data_.name.empty()
					? L"Listening on %s (%s)."
					: L"Listening on %s (%s) [%s].";

				logger_.log_u(logmsg::status, status_log, address_info_.address, listen_socket_->local_ip(), user_data_.name);

				status_ = status::started;
			}
			else
				logger_.log_u(logmsg::error, L"Couldn't create listener for %s. Reason: %s.", address_info_.address, socket_error_description(error));
		}
		else {
			listen_socket_ = std::make_unique<listen_socket>(pool_, static_cast<event_handler *>(const_cast<listener *>(this)));
			try_listen();
		}
	}
}

void fz::tcp::listener::stop()
{
	fz::scoped_lock lock(mutex_);

	status_ = status::stopped;

	if (listen_socket_) {
		const_cast<listener*>(this)->stop_timer(timer_id_);
		listen_socket_.reset();
		fz::remove_events<connected_event>(&target_handler_, *this);
		accepted_.clear();

		status_change_notifier_.listener_status_changed(*this);
	}
}

void fz::tcp::listener::operator ()(const fz::event_base &ev) {

	{
		fz::scoped_lock lock(mutex_);

		if (status_ == status::stopped)
			return;
	}

	fz::dispatch<
		socket_event,
		timer_event
	>(ev, this,
		&listener::on_socket_event,
		&listener::on_timer_event
	);
}

void fz::tcp::listener::on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error) {
	if (type == fz::socket_event_flag::connection) {
		if (error) {
			logger_.log_u(fz::logmsg::error, L"[%s] Error during connection on %s. Reason: %s.", join_host_and_port(address_info_.address, address_info_.port), socket_error_description(error), error);
			return;
		}

		auto socket = listen_socket_->accept(error);

		if (!socket) {
			logger_.log_u(fz::logmsg::error, L"Failed to accept new connection on %s. Reason: %s.", join_host_and_port(address_info_.address, address_info_.port), socket_error_description(error), error);
			return;
		}

		if (!peer_allowance_checker_.is_peer_allowed(socket->peer_ip(), socket->address_family())) {
			logger_.log_u(fz::logmsg::warning, L"Peer %s is not allowed.",  socket->peer_ip());
			return;
		}

		bool send_event = false;

		{
			fz::scoped_lock lock(mutex_);
			accepted_.push_back(std::move(socket));

			// When an event gets sent, it's responsibility of the receiving handler to empty the accepted sockets queue.
			if (accepted_.size() == 1) {
				send_event = true;
			}
		}

		if (send_event) {
			target_handler_.send_event<connected_event>(*this);
		}
   }
}

void fz::tcp::listener::on_timer_event(const fz::timer_id &) {
	try_listen();
}

void fz::tcp::listener::try_listen() {
	constexpr static int retry_time = 1;

	int error = !listen_socket_->bind(address_info_.address) ? EBADF : listen_socket_->listen(fz::address_type::unknown, (int)address_info_.port);

	timer_id_ = {};

	if (error) {
		logger_.log_u(logmsg::error, L"Couldn't bind on %s. Reason: %s. Retrying in %d seconds.", join_host_and_port(address_info_.address, address_info_.port), socket_error_description(error), retry_time);
		timer_id_ = const_cast<listener *>(this)->add_timer(fz::duration::from_seconds(retry_time), true);

		if (status_ != status::retrying_to_start) {
			status_ = status::retrying_to_start;
			status_change_notifier_.listener_status_changed(*this);
		}
	}
	else {
		auto status_log = user_data_.name.empty()
			? L"Listening on %s."
			: L"Listening on %s [%s].";

		logger_.log_u(logmsg::status, status_log, join_host_and_port(address_info_.address, address_info_.port), user_data_.name);

		status_ = status::started;
		status_change_notifier_.listener_status_changed(*this);
	}
}

fz::tcp::listener::peer_allowance_checker::~peer_allowance_checker()
{
}

fz::tcp::listener::status_change_notifier::~status_change_notifier()
{
}

const fz::tcp::listener::peer_allowance_checker &fz::tcp::listener::peer_allowance_checker::allow_all = []() -> const fz::tcp::listener::peer_allowance_checker & {
	const static struct allow_all: fz::tcp::listener::peer_allowance_checker
	{
		bool is_peer_allowed(std::string_view, fz::address_type) const override
		{
			return true;
		}
	} allow_all;

	return allow_all;
}();

fz::tcp::listener::status_change_notifier &fz::tcp::listener::status_change_notifier::none = []() -> fz::tcp::listener::status_change_notifier & {
	static struct none: fz::tcp::listener::status_change_notifier
	{
		void listener_status_changed(const listener &) override
		{
		}
	} none;

	return none;
}();


fz::tcp::listeners_manager::listeners_manager(thread_pool &pool,
											event_loop &loop,
											event_handler &target_handler,
											logger_interface &logger,
											const listener::peer_allowance_checker &pac,
											listener::status_change_notifier &scn)
	: pool_(pool)
	, loop_(loop)
	, target_handler_(target_handler)
	, logger_(logger)
	, peer_allowance_checker_(pac)
	, status_change_notifier_(scn)
{
}

bool fz::tcp::listeners_manager::start()
{
	scoped_lock lock(mutex_);

	if (is_running_) {
		return false;
	}

	is_running_ = true;

	for (auto it = listeners_.begin(); it != listeners_.end();) {
		auto &&node = listeners_.extract(it++);

		node.value().start();

		listeners_.insert(std::move(node));
	}

	return true;
}

bool fz::tcp::listeners_manager::stop()
{

	scoped_lock lock(mutex_);

	if (!is_running_) {
		return false;
	}

	is_running_ = false;

	logger_.log_u(logmsg::debug_debug, L"Stopping listeners.");
	for (auto it = listeners_.begin(); it != listeners_.end();) {
		auto &&node = listeners_.extract(it++);

		node.value().stop();

		listeners_.insert(std::move(node));
	}

	return true;
}

bool fz::tcp::listeners_manager::is_running() const
{
	scoped_lock lock(mutex_);
	return is_running_;
}
