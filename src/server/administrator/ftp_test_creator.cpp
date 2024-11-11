#include "ftp_test_creator.hpp"

administrator::ftp_test_creator::ftp_test_creator(administrator &admin)
	: fz::event_handler(admin.server_context_.loop())
	, admin_(admin)
	, logger_(admin_.logger_, "FTP Test Creator")
{
}

administrator::ftp_test_creator::~ftp_test_creator()
{
	remove_handler();
	destroy_environment();
}

std::pair<std::string, std::string> administrator::ftp_test_creator::create_environment(fz::ftp::server::options ftp_opts, fz::duration timeout)
{
	auto ret = admin_.authenticator_.make_temp_user({
		{"/this_is_a_test", {}, fz::tvfs::mount_point::disabled},
	});

	if (ret.first.empty()) {
		logger_.log_raw(fz::logmsg::error, L"Couldn't create a temporary user for the FTP test.");
		return {};
	}

	auto &cert = ftp_opts.sessions().tls.cert;

	if (cert) {
		if (!admin_.blob_obfuscator_.deobfuscate(cert) && admin_.blob_obfuscator_.is_obfuscated(cert)) {
			logger_.log_raw(fz::logmsg::error, L"Couldn't deobfuscate the FTP certificate key. Cannot create FTP test.");
			return {};
		}

		if (!cert.set_root_path(admin_.config_paths_.certificates(), &logger_)) {
			logger_.log_raw(fz::logmsg::error, L"Couldn't activate the FTP certificate. Cannot create FTP test.");
			return {};
		}
	}

	destroy_environment();

	temp_username_ = ret.first;
	previous_ftp_opts_ = admin_.server_settings_.lock()->ftp_server;
	admin_.ftp_server_.set_options(std::move(ftp_opts));
	timer_id_ = add_timer(timeout, true);

	return ret;
}

bool administrator::ftp_test_creator::destroy_environment()
{
	if (timer_id_) {
		stop_timer(timer_id_);
		do_destroy_environment();
		return true;
	}

	return false;
}

void administrator::ftp_test_creator::operator()(const fz::event_base &ev)
{
	fz::dispatch<fz::timer_event>(ev, [&](auto &) {
		do_destroy_environment();
	});
}

void administrator::ftp_test_creator::do_destroy_environment()
{
	admin_.authenticator_.remove_temp_user(temp_username_);
	admin_.ftp_server_.set_options(std::move(previous_ftp_opts_));
	timer_id_ = {};
}

auto administrator::operator()(administration::create_ftp_test_environment &&v)
{
	auto &&[ftp_opts, timeout] = std::move(v.tuple());

	auto res = ftp_test_creator_->create_environment(std::move(ftp_opts), timeout);

	if (!res.first.empty()) {
		return v.success(std::move(res.first), std::move(res.second));
	}

	return v.failure();
}

auto administrator::operator()(administration::destroy_ftp_test_environment &&v)
{
	bool res = ftp_test_creator_->destroy_environment();

	if (res) {
		return v.success();
	}

	return v.failure();
}

FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::create_ftp_test_environment);
FZ_RMP_INSTANTIATE_HERE_DISPATCHING_FOR(administration::engine, administrator, administration::destroy_ftp_test_environment);
