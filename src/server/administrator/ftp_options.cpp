#include "../administrator.hpp"
#include "ftp_test_creator.hpp"

auto administrator::operator()(administration::set_ftp_options &&v)
{
	auto &&[opts] = std::move(v).tuple();

	if (!blob_obfuscator_.deobfuscate(opts.sessions().tls.cert) && blob_obfuscator_.is_obfuscated(opts.sessions().tls.cert)) {
		logger_.log_raw(fz::logmsg::error, L"Couldn't deobfuscate the FTP certificate key. The FTP options will not be applied.");
		return v.failure();
	}

	set_ftp_options(std::move(opts));

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_ftp_options &&v)
{
	auto && [export_cert] = v.tuple();

	auto s = server_settings_.lock();

	auto ftp_server = fz::ftp::server::options(s->ftp_server);

	if (export_cert) {
		ftp_server.sessions().tls.cert = s->ftp_server.sessions().tls.cert.generate_exported();
	}
	else {
		blob_obfuscator_.obfuscate(ftp_server.sessions().tls.cert);
	}

	return v.success(ftp_server, ftp_server.sessions().tls.cert.load_extra(&logger_));
}

void administrator::set_ftp_options(fz::ftp::server::options &&opts)
{
	ftp_test_creator_->destroy_environment();

	auto server_settings = server_settings_.lock();

	set_acme_certificate_for_renewal(get_ftp_cert, false);

	server_settings->ftp_server = std::move(opts);

	if (server_settings->ftp_server.sessions().tls.cert) {
		server_settings->ftp_server.sessions().tls.cert.set_root_path(config_paths_.certificates());
		set_acme_certificate_for_renewal(get_ftp_cert, true);
	}

	ftp_server_.set_options(server_settings->ftp_server);
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_ftp_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_ftp_options);
