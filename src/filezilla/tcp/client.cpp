#include "../tcp/client.hpp"

namespace fz::tcp {

client::client(thread_pool &pool, event_loop &loop, logger_interface &logger, session::factory &session_factory)
	: invoker_handler(loop)
	, pool_(pool)
	, logger_(logger)
	, session_factory_(session_factory)
{}

client::~client() {
	remove_handler();
}

void client::connect(address_info address_info, std::any user_data)
{
	invoke_later([this, address_info = std::move(address_info), user_data = std::move(user_data)]() mutable {
		int err = EALREADY;

		fz::scoped_lock lock(mutex_);

		if (!session_) {
			address_info_ = std::move(address_info);
			user_data_ = std::move(user_data);

			socket_ = std::make_unique<socket>(pool_, static_cast<event_handler *>(this));
			err = socket_->connect(fz::to_native(address_info_.address), address_info_.port);
		}

		if (err)
			send_event<fz::socket_event>(socket_.get(), fz::socket_event_flag::connection, err);
	});
}

void client::disconnect() {
	fz::scoped_lock lock(mutex_);
	if (session_)
		session_->shutdown();
}

bool client::is_connected() const {
	fz::scoped_lock lock(mutex_);

	return session_ && session_->is_alive();
}

void client::operator ()(const event_base &ev)
{
	if (on_invoker_event(ev))
		return;

	fz::dispatch<
		socket_event,
		session::ended_event
	>(ev, this,
		&client::on_socket_event,
		&client::on_session_ended_event
	);
}

void client::on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error)
{
	if (type == socket_event_flag::connection) {
		if (error)
			logger_.log_u(logmsg::error, L"Client failed connection. Reason: %s.", socket_error_description(error));

		auto session = session_factory_.make_session(*this, 0, std::move(socket_), user_data_, error);

		fz::scoped_lock lock(mutex_);
		session_ = std::move(session);
	}
}

void client::on_session_ended_event(session::id, const channel::error_type &error)
{
	if (error && error != ECONNRESET)
		logger_.log_u(logmsg::error, "Session ended with error from source %d. Reason: %s.", (int)error.source(), socket_error_description(error.error()));

	// Use a temporary variable to hold the session pointer, 'cause we don't want to hold the lock while the session is being destroyed.
	decltype(session_) session;
	fz::scoped_lock lock(mutex_);
	session_.swap(session);
}

}
