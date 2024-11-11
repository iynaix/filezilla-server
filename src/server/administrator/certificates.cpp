#include "../administrator.hpp"

auto administrator::operator()(administration::generate_selfsigned_certificate &&v, administration::engine::session &)
{
	auto [distinguished_name, hostnames, key, password] = std::move(v).tuple();

	fz::logger::splitter splitter;
	splitter.set_all(fz::logmsg::type(~0));

	fz::native_string error;
	fz::native_string_logger string_logger(error, fz::logmsg::error);

	splitter.add_logger(logger_);
	splitter.add_logger(string_logger);

	if (!blob_obfuscator_.deobfuscate(key) && blob_obfuscator_.is_obfuscated(key)) {
		return v.failure(fzT("Could not deobfuscate the private key."));
	}

	auto cert = fz::securable_socket::cert_info::generate_selfsigned(std::move(key), config_paths_.certificates(), splitter, password, distinguished_name, hostnames);

	if (cert) {
		auto extra = cert.load_extra(&splitter);

		if (extra) {
			blob_obfuscator_.obfuscate(cert);

			return v.success(std::move(cert), std::move(extra));
		}
	}

	return v.failure(std::move(error));
}

auto administrator::operator()(administration::get_extra_certs_info &&v, administration::engine::session &)
{
	auto && [info] = std::move(v).tuple();

	fz::native_string error;
	fz::native_string_logger logger(error, fz::logmsg::error);
	fz::securable_socket::cert_info::extra extra;

	if (!blob_obfuscator_.deobfuscate(info) && blob_obfuscator_.is_obfuscated(info)) {
		return v.failure(fzT("Could not deobfuscate the private key."));
	}

	if (info.set_root_path(config_paths_.certificates(), &logger)) {
		error = fz::check_key_and_certs_status(info.key(), info.certs(), info.key_password());

		if (error.empty()) {
			extra = info.load_extra(&logger);

			if (extra) {
				return v.success(std::move(extra));
			}
			else {
				error = fzT("Internal consistency error");
			}
		}
	}

	return v.failure(std::move(error));
}

auto administrator::operator()(administration::get_deobfuscated_blob &&v, administration::engine::session &)
{
	auto && [blob] = v.tuple();

	if (!blob_obfuscator_.deobfuscate(blob) && blob_obfuscator_.is_obfuscated(blob)) {
		return v.failure();
	}

	return v.success(blob);
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::generate_selfsigned_certificate);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_extra_certs_info);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::get_deobfuscated_blob);

