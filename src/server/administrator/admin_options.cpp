#include "../administrator.hpp"

auto administrator::operator()(administration::set_admin_options &&v, administration::engine::session &session)
{
	auto &&[opts] = std::move(v).tuple();

	if (!blob_obfuscator_.deobfuscate(opts.tls.cert) && blob_obfuscator_.is_obfuscated(opts.tls.cert)) {
		logger_.log_raw(fz::logmsg::error, L"Couldn't deobfuscate the Administration certificate key. The Adminstration options will not be applied.");
		return v.failure();
	}

	set_admin_options(std::move(opts));

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_admin_options &&v, administration::engine::session &session)
{
	auto && [export_cert] = std::move(v).tuple();

	auto s = server_settings_.lock();

	auto admin = server_settings::admin_options(s->admin);

	if (export_cert) {
		admin.tls.cert = admin.tls.cert.generate_exported();
	}
	else {
		blob_obfuscator_.obfuscate(admin.tls.cert);
	}

	return v.success(admin, admin.tls.cert.load_extra(&logger_));
}

void administrator::set_admin_options(server_settings::admin_options &&opts)
{
	{
		auto server_settings = server_settings_.lock();

		set_acme_certificate_for_renewal(get_admin_cert, false);
		server_settings->admin = std::move(opts);
		set_acme_certificate_for_renewal(get_admin_cert, true);
	}

	handle_new_admin_settings();
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_admin_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_admin_options);
