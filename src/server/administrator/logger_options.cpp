#include "../administrator.hpp"

auto administrator::operator()(administration::set_logger_options &&v)
{
	auto && [opts] = std::move(v).tuple();
	set_logger_options(std::move(opts));

	/*****/

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_logger_options &&v)
{
	return v.success(server_settings_.lock()->logger);
}


void administrator::set_logger_options(fz::logger::file::options &&opts)
{
	auto server_settings = server_settings_.lock();

	server_settings->logger = std::move(opts);

	file_logger_.set_options(server_settings->logger);
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_logger_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_logger_options);
