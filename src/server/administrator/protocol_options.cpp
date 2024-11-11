#include "../administrator.hpp"

auto administrator::operator()(administration::set_protocols_options &&v)
{
	auto &&[opts] = std::move(v).tuple();

	set_protocols_options(std::move(opts));

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_protocols_options &&v)
{
	auto s = server_settings_.lock();

	return v.success(s->protocols);
}

void administrator::set_protocols_options(server_settings::protocols_options &&opts)
{
	auto server_settings = server_settings_.lock();

	auto &p = server_settings->protocols = std::move(opts);

	autobanner_.set_options(p.autobanner);
	loop_pool_.set_max_num_of_loops(p.performance.number_of_session_threads);
	ftp_server_.set_data_buffer_sizes(p.performance.receive_buffer_size, p.performance.send_buffer_size);
	ftp_server_.set_timeouts(p.timeouts.login_timeout, p.timeouts.activity_timeout);
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_protocols_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_protocols_options);
