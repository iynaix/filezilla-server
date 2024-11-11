#include "../administrator.hpp"

auto administrator::operator()(administration::set_pkcs11_options &&v)
{
	auto && [opts] = std::move(v).tuple();
	set_pkcs11_options(std::move(opts));

	/*****/

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_pkcs11_options &&v)
{
	return v.success(server_settings_.lock()->pkcs11);
}

void administrator::set_pkcs11_options(server_settings::pkcs11_options &&opts)
{
	auto server_settings = server_settings_.lock();

	if (opts != server_settings->pkcs11) {
		logger_.log_raw(fz::logmsg::warning, L"The PKCS#11 options have changed. The server must be restarted for them to have effect.");
		server_settings->pkcs11 = std::move(opts);
	}
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_pkcs11_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_pkcs11_options);
