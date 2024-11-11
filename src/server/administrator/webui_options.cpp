#include "../administrator.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

void administrator::set_webui_options(fz::webui::server::options &&opts)
{
	auto server_settings = server_settings_.lock();

	set_acme_certificate_for_renewal(get_webui_cert, false);

	server_settings->webui = std::move(opts);

	if (server_settings->webui.tls.cert) {
		server_settings->webui.tls.cert.set_root_path(config_paths_.certificates());
		set_acme_certificate_for_renewal(get_webui_cert, true);
	}

#ifdef ENABLE_FZ_WEBUI
	webui_server_.set_options(server_settings->webui);
#endif
}

#ifdef ENABLE_FZ_WEBUI

auto administrator::operator()(administration::set_webui_options &&v)
{
	auto &&[opts] = std::move(v).tuple();

	if (!blob_obfuscator_.deobfuscate(opts.tls.cert) && blob_obfuscator_.is_obfuscated(opts.tls.cert)) {
		logger_.log_raw(fz::logmsg::error, L"Couldn't deobfuscate the WebUI certificate key. The WebUI options will not be applied.");
		return v.failure();
	}

	set_webui_options(std::move(opts));

	server_settings_.save_later();

	return v.success();
}

auto administrator::operator()(administration::get_webui_options &&v)
{
	auto && [export_cert] = v.tuple();

	auto s = server_settings_.lock();

	auto webui = fz::webui::server::options(s->webui);

	if (export_cert) {
		webui.tls.cert = s->webui.tls.cert.generate_exported();
	}
	else {
		blob_obfuscator_.obfuscate(webui.tls.cert);
	}

	return v.success(webui, webui.tls.cert.load_extra(&logger_));
}

auto administrator::operator()(administration::destroy_webui_tokens &&v)
{
	webui_server_.reset_tokens();

	return v.success();
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_webui_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::set_webui_options);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::destroy_webui_tokens);

#endif // ENABLE_FZ_WEBUI
