#include "../../src/filezilla/update/info_retriever/chain.hpp"
#include "../../src/filezilla/update/raw_data_retriever/http.hpp"

#include "update_checker.hpp"

administrator::update_checker::admin_retriever::admin_retriever(update_checker &checker, fz::logger_interface &logger)
	: event_handler(checker.admin_.server_context_.loop())
	, checker_(checker)
	, logger_(logger, "Admin retriever")
{
}

administrator::update_checker::admin_retriever::~admin_retriever()
{
	remove_handler();
}

void administrator::update_checker::admin_retriever::retrieve_raw_data(bool manual, fz::receiver_handle<result> h)
{
	if (handle_)
		return h(fz::unexpected("Admin Retriever: operation already in progress."));

	handle_ = std::move(h);
	manual_ = manual;
	remaining_attempts_ = checker_.admin_.admin_server_.get_number_of_sessions();

	send_request();
}

void administrator::update_checker::admin_retriever::send_request()
{
	auto [err, session_id] = checker_.admin_.admin_server_.send_to_random_client<administration::retrieve_update_raw_data>(
		fz::update::raw_data_retriever::http::get_query_string(manual_)
	);

	if (err) {
		if (err != ENOTCONN || checker_.admin_.admin_server_.get_number_of_sessions() > 0)
			handle_response(fz::unexpected(fz::sprintf("Couldn't send request to admin client: %s.", fz::socket_error_description(err))));
		else {
			logger_.log_raw(fz::logmsg::debug_info, L"No Administrator interface is connected, cannot retrieve data.");
			handle_response(std::string());
		}

		return;
	}

	logger_.log_u(fz::logmsg::debug_info, L"Sending message to admin client with id %d", session_id);

	session_id_ = session_id;
	timer_id_ = add_timer(fz::update::raw_data_retriever::http::response_timeout + fz::duration::from_seconds(10), true);
}

void administrator::update_checker::admin_retriever::operator()(const fz::event_base &ev)
{
	fz::dispatch<fz::timer_event>(ev, [&](fz::timer_id id) {
		if (id != timer_id_)
			return;

		timer_id_ = {};

		checker_.admin_.admin_server_.end_sessions({session_id_}, ETIMEDOUT);
	});
}

void administrator::update_checker::admin_retriever::handle_response(administration::retrieve_update_raw_data::response &&v, administration::engine::session::id id)
{
	if (!handle_ || id != session_id_)
		return;

	stop_timer(timer_id_);
	session_id_ = {};
	timer_id_ = {};

	auto &&[expected_data] = std::move(v).tuple();

	if (!expected_data) {
		assert(remaining_attempts_ > 0);

		logger_.log_u(fz::logmsg::debug_warning, L"%s", expected_data.error());
		remaining_attempts_ -= 1;

		if (remaining_attempts_ == 0)
			return handle_(fz::unexpected("All attempts to retrieve data from the connected admin client(s) failed."));

		logger_.log(fz::logmsg::debug_info, L"Trying again. Got %d attempts left.", remaining_attempts_);
		return send_request();
	}

	if (!expected_data->empty())
		logger_.log_raw(fz::logmsg::debug_info, L"Got valid data from one of the connected admin clients.");

	handle_(std::move(std::move(*expected_data)));
}

void administrator::update_checker::admin_retriever::on_disconnection(administration::engine::session::id id)
{
	if (id != session_id_)
		return;

	handle_response(fz::unexpected(fz::sprintf("Administration client with id %d got disconnected.", id)), id);
}

administrator::update_checker::update_checker(administrator &admin, const fz::native_string &cachepath, options opts)
	: event_handler(admin.server_context_.loop())
	, admin_(admin)
	, cachepath_(cachepath)
	, logger_(admin.logger_, "Update checker")
	, http_retriever_(admin.server_context_.loop(), admin.server_context_.pool(), logger_)
	, admin_retriever_(*this, logger_)
	, chain_(logger_, [&](bool manual) -> fz::update::info_retriever::chain::raw_data_retrievers {
		if (manual || checker_.get_options().frequency())
			return { &http_retriever_, &admin_retriever_};

		return {};
	})
	, checker_(admin.server_context_.loop(), chain_, *this, cachepath, logger_)
{
	set_options(std::move(opts));
}

administrator::update_checker::~update_checker()
{
	remove_handler_and_stop_receiving();
}

void administrator::update_checker::operator()(const fz::event_base &ev)
{
	fz::dispatch<fz::update::checker::result>(ev, [&](auto &expected_info, fz::datetime last_check, fz::datetime next_check) {
		if (expected_info && expected_info->is_eol()) {
			// If EOL, the checker will not perform automatic checks.
			logger_.log_u(fz::logmsg::warning, L"The version of %s you are running has reached its End Of Life and is not supported anymore. Automatic update checks are disabled.", fz::build_info::package_name);
		}

		admin_.admin_server_.broadcast<administration::update_info>(std::move(expected_info), last_check, next_check);
	});
}

void administrator::update_checker::set_options(options opts)
{
	if (!opts.frequency())
		logger_.log_raw(fz::logmsg::status, L"Automatic update checking is disabled.");
	else
	if (opts.frequency() < fz::duration::from_days(7)) {
		logger_.log_raw(fz::logmsg::warning, L"Automatic update checking frequency is set to less than 7 days, which is not allowed. Forcefully setting it to 7 days now.");
		opts.frequency() = fz::duration::from_days(7);
	}

	chain_.set_expiration(opts.frequency());
	checker_.set_options(std::move(opts));
}

void administrator::update_checker::start()
{
	checker_.start();
}

void administrator::update_checker::stop()
{
	checker_.stop();
}

bool administrator::update_checker::check_now()
{
	return checker_.check_now();
}

void administrator::update_checker::handle_response(administration::retrieve_update_raw_data::response &&v, administration::engine::session::id id)
{
	admin_retriever_.handle_response(std::move(v), id);
}

void administrator::update_checker::on_disconnection(administration::engine::session::id id)
{
	admin_retriever_.on_disconnection(id);
}

fz::update::info administrator::update_checker::get_last_checked_info() const
{
	return checker_.get_last_checked_info();
}

fz::datetime administrator::update_checker::get_last_check_dt() const
{
	return checker_.get_last_check_dt();
}

fz::datetime administrator::update_checker::get_next_check_dt() const
{
	return checker_.get_next_check_dt();
}

void administrator::set_updates_options(fz::update::checker::options &&opts)
{
	server_settings_.lock()->update_checker = opts;

	if (update_checker_)
		update_checker_->set_options(std::move(opts));
}

#if !defined(WITHOUT_FZ_UPDATE_CHECKER)
// This will be solely received by any of the Administrator client that was asked to retrieve the update raw data for us.
auto administrator::operator()(administration::retrieve_update_raw_data::response &&v, administration::engine::session &session)
{
	if (update_checker_)
		update_checker_->handle_response(std::move(v), session.get_id());
}

auto administrator::operator()(administration::solicit_update_info &&)
{
	if (update_checker_)
		update_checker_->check_now();
}

auto administrator::operator()(administration::set_updates_options &&v)
{
	auto &&[opts] = std::move(v).tuple();

	set_updates_options(std::move(opts));

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_updates_options &&v)
{
	return v.success(server_settings_.lock()->update_checker);
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_updates_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_updates_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::solicit_update_info);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::retrieve_update_raw_data::response);
#endif
