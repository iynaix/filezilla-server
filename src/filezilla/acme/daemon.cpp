#include <libfilezilla/jws.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/recursive_remove.hpp>

#include "../string.hpp"
#include "../util/bits.hpp"
#include "../serialization/archives/xml.hpp"
#include "../serialization/types/time.hpp"

#include "daemon.hpp"

namespace fz::acme
{

namespace {

	auto maximum_allowed_time_difference_from_acme_server = fz::duration::from_days(1);

	std::string id_of(tls_param p) {
		return fz::base32_encode(fz::to_string(p.url()), base32_type::locale_safe, false);
	}

}

class daemon::worker: public event_handler
{
public:
	worker(daemon &daemon, std::string name, securable_socket::omni_cert_info ci, create_certificate_handler handler);
	~worker() override;

	void schedule_renewal();

	const fz::tls_param &certs() const
	{
		return info_.certs;
	}

	const std::string &name() const
	{
		return name_;
	}

private:
	void load_status();
	bool save_status();
	void trash_status();
	void remove_status();
	void restore_trashed_status();

	void renew_now();

	void on_certificate(acme::client::opid_t, uri &, securable_socket::certs_and_key &ck, securable_socket::certs_and_key::sources::acme &sa);
	void on_error(acme::client::opid_t i, const native_string &error, datetime retry_at);
	void on_timer(timer_id);

	void operator()(const event_base &ev) override;

private:
	daemon &daemon_;
	logger::modularized logger_;
	securable_socket::omni_cert_info info_;
	create_certificate_handler handler_;

	std::string name_;
	std::vector<std::string> hostnames_;
	datetime renewal_dt_{};
	std::size_t error_count_{};
	timer_id timer_id_{};
};

daemon::worker::worker(daemon &daemon,  std::string name, securable_socket::omni_cert_info ci, create_certificate_handler handler)
	: event_handler(daemon.event_loop_)
	, daemon_(daemon)
	, logger_(daemon.logger_, fz::sprintf("Certificate Renewer (%s)", name))
	, info_(std::move(ci))
	, handler_(std::move(handler))
	, name_(std::move(name))
{
	logger_.set_meta({{"ID", id_of(info_.certs)}});

	restore_trashed_status();
	load_status();
}

daemon::worker::~worker()
{
	remove_handler();
	trash_status();
}

void daemon::worker::on_timer(timer_id)
{
	timer_id_ = {};
	renew_now();
}

void daemon::worker::on_error(acme::client::opid_t, const native_string &error, datetime retry_at)
{
	logger_.log_u(logmsg::error, L"Finished renewal of certificate for the domains [%s], registered with the account %s. FAILED: %s", fz::join<std::string>(hostnames_), info_.acme()->account_id, error);

	if (retry_at) {
		logger_.log_u(logmsg::error, L"The ACME server instructed us to try again on date %s.", retry_at.get_rfc822());

		renewal_dt_ = retry_at;
		error_count_ = 0;
	}
	else
	if (error_count_ != std::size_t(-1)) {
		error_count_ += 1;
	}

	save_status();
	schedule_renewal();
}

void daemon::worker::on_certificate(acme::client::opid_t id, fz::uri &, securable_socket::certs_and_key &ck, securable_socket::certs_and_key::sources::acme &sa)
{
	sa.autorenew = true;

	auto o = securable_socket::omni_cert_info{ std::move(ck), std::move(sa) };
	securable_socket::cert_info ci = o;

	{
		scoped_lock lock(daemon_.mutex_);

		if (!ci.set_root_path(daemon_.root_path_, &logger_)) {
			on_error(id, fzT("An error occurred while verifying the key and certs."), {});
			return;
		}

		remove_status();

		info_ = o;
	}

	renewal_dt_ = {};
	error_count_ = 0;

	auto cert_id = id_of(o.certs);

	logger_.log_u(logmsg::status, L"Finished renewal of certificate for the domains [%s], registered with the account %s. SUCCESS.\n"
		L"New certificate is: %s (ID: %s).",
		fz::join<std::string>(hostnames_), o.acme()->account_id,
		o.certs.url(), cert_id
	);

	logger_.set_meta({{"ID", cert_id}});

	handler_(ci);

	schedule_renewal();
}

void daemon::worker::operator()(const event_base &ev)
{
	fz::dispatch<
		acme::client::error_event,
		acme::client::certificate_event,
		timer_event
	>(ev, this,
		&worker::on_error,
		&worker::on_certificate,
		&worker::on_timer
	);
}

daemon::daemon(thread_pool &pool, event_loop &loop, logger_interface &logger, tls_system_trust_store &trust_store)
	: fz::event_handler(loop)
	, pool_(pool)
	, logger_(logger, "ACME Daemon")
	, trust_store_(trust_store)
	, client_(make_new_client())
{
}

daemon::~daemon()
{
	remove_handler();
	empty_status_trash();
}

void daemon::set_root_path(const util::fs::native_path &root_path)
{
	fz::scoped_lock lock(mutex_);
	root_path_ = root_path;

	try_to_renew_expiring_certs();
}

void daemon::set_how_to_serve_challenges(const serve_challenges::how &how_to_serve_challenges)
{
	fz::scoped_lock lock(mutex_);
	how_to_serve_challenges_ = how_to_serve_challenges;

	try_to_renew_expiring_certs();
}

void daemon::set_certificate(std::string name, securable_socket::cert_info ci, create_certificate_handler ch)
{
	{
		decltype(workers_) to_erase;

		auto erase_if = [&](const auto &p) {
			for (auto it = workers_.begin(); it != workers_.end();) {
				if (p(*it)) {
					to_erase.splice(to_erase.end(), workers_, it++);
				}
				else {
					++it;
				}
			}
		};

		{
			fz::scoped_lock lock(mutex_);

			if (name.empty()) {
				to_erase.swap(workers_);
			}
			else
			if (auto omni = ci.omni(); !omni || !omni->acme()) {
				erase_if([&](auto &w) {
					return w.name() == name;
				});
			}
			else {
				erase_if([&](auto &w) {
					return w.name() == name && omni->certs == w.certs();
				});

				if (omni->acme()->autorenew && ch) {
					workers_.emplace_back(*this, std::move(name), std::move(*omni), std::move(ch));
				}
			}

			if (!to_erase.empty()) {
				for (auto &h: id2handlers_) {
					std::visit([](auto &h) {
						h.second(fzT("acme::daemon: operation was halted."));
					}, h.second);
				}

				id2handlers_.clear();

				client_ = make_new_client();
			}
		}

	}

	try_to_renew_expiring_certs();
}

void daemon::set_certificate(std::pair<std::string, securable_socket::cert_info> name_and_ci, create_certificate_handler ch)
{
	set_certificate(std::move(name_and_ci.first), std::move(name_and_ci).second, std::move(ch));
}

void daemon::get_terms_of_service(const fz::uri &directory, daemon::get_terms_of_service_handler terms_handler, daemon::error_handler error_handler)
{
	fz::scoped_lock lock(mutex_);

	auto id = client_->get_terms_of_service(directory, *this);
	if (!id) {
		if (error_handler)
			error_handler(fzT("Could not execute acme::client::get_terms_of_service: there's already an operation in progress."));
		return;
	}

	id2handlers_[id] = terms_handlers{ std::move(terms_handler), std::move(error_handler) };
}

void daemon::create_account(const uri &directory, const std::vector<std::string> &contacts, daemon::create_account_handler account_handler, daemon::error_handler error_handler)
{
	fz::scoped_lock lock(mutex_);

	if (!root_path_.is_absolute()) {
		if (error_handler)
			error_handler(fzT("acme::daemon: root path is not absolute."));
		return;
	}

	auto id = client_->get_account(directory, contacts, create_jwk(), false, *this);
	if (!id) {
		if (error_handler)
			error_handler(fzT("Could not execute acme::client::get_account: there's already an operation in progress."));
		return;
	}

	id2handlers_[id] = acct_handlers{ std::move(account_handler), std::move(error_handler) };
}

void daemon::restore_account(const std::string &account_id, const extra_account_info &extra, restore_account_handler restore_handler, error_handler error_handler)
{
	if (!root_path_.is_absolute()) {
		if (error_handler)
			error_handler(fzT("acme::daemon: root path is not absolute."));
		return;
	}

	if (!extra.save(root_path_, account_id)) {
		if (error_handler)
			error_handler(fzT("acme::daemon: failed restoring account."));
		return;
	}

	if (restore_handler)
		restore_handler();

	return;
}

void daemon::create_certificate(const std::string &account_id,
	const serve_challenges::how &how_to_serve_challenges,
	const std::vector<std::string> &hosts,
	tls_param key, native_string password,
	fz::duration allowed_max_server_time_difference,
	daemon::create_certificate_handler cert_handler, daemon::error_handler error_handler
)
{
	fz::scoped_lock lock(mutex_);

	if (!root_path_.is_absolute()) {
		if (error_handler)
			error_handler(fzT("acme::daemon: root path is not absolute."));
		return;
	}

	extra_account_info extra = load_extra_account_info(account_id);
	if (!extra)
		error_handler(fzT("acme::deaemon: could not read or parse account info"));

	auto id = client_->get_certificate(fz::uri(extra.directory), extra.jwk, hosts, std::move(key), std::move(password), how_to_serve_challenges, allowed_max_server_time_difference, *this);
	if (!id) {
		if (error_handler) {
			error_handler(fzT("Could not execute acme::client::get_account: there's already an operation in progress."));
		}

		return;
	}

	id2handlers_[id] = cert_handlers{ std::move(cert_handler), std::move(error_handler) };
}

extra_account_info daemon::load_extra_account_info(const std::string &account_id) const
{
	return extra_account_info::load(root_path_, account_id);
}

void daemon::on_terms(acme::client::opid_t id, std::string &terms)
{
	get_terms_of_service_handler th;

	{
		fz::scoped_lock lock(mutex_);
		if (auto it = id2handlers_.find(id); it != id2handlers_.end()) {
			th = std::move(std::get_if<terms_handlers>(&it->second)->first);

			id2handlers_.erase(it);
		}
	}

	if (th) {
		th(terms);
	}
}

void daemon::on_certificate(acme::client::opid_t id, fz::uri &, securable_socket::certs_and_key &ck, securable_socket::certs_and_key::sources::acme &sa)
{
	create_certificate_handler ch;
	error_handler eh;

	fz::scoped_lock lock(mutex_);

	if (auto it = id2handlers_.find(id); it != id2handlers_.end()) {
		ch = std::move(std::get_if<cert_handlers>(&it->second)->first);
		eh = std::move(std::get_if<cert_handlers>(&it->second)->second);

		id2handlers_.erase(it);
	}

	securable_socket::cert_info ci = securable_socket::omni_cert_info{ std::move(ck), std::move(sa) };
	if (!ci.set_root_path(root_path_, &logger_)) {
		if (eh) {
			eh(fzT("An error occurred while verifying the key and certs."));
		}
	}

	if (ch) {
		ch(std::move(ci));
	}
}

void daemon::on_error(acme::client::opid_t id, const native_string &error, datetime retry_at)
{
	error_handler h;

	{
		fz::scoped_lock lock(mutex_);
		if (auto it = id2handlers_.find(id); it != id2handlers_.end()) {
			h = std::visit([](auto &handlers) { return std::move(handlers.second); }, it->second);

			id2handlers_.erase(it);
		}
	}

	if (h)
		h(error_type{error, retry_at});
}

void daemon::on_account(acme::client::opid_t id, fz::uri &directory, std::string &kid, std::pair<json, json> &jwk, json &object)
{
	create_account_handler ah;
	error_handler eh;

	fz::scoped_lock lock(mutex_);

	if (auto it = id2handlers_.find(id); it != id2handlers_.end()) {
		ah = std::move(std::get_if<acct_handlers>(&it->second)->first);
		eh = std::move(std::get_if<acct_handlers>(&it->second)->second);

		id2handlers_.erase(it);
	}

	json info;
	info["directory"] = std::move(directory.to_string());
	info["kid"] = kid;
	info["jwk"]["priv"] = std::move(jwk.first);
	info["jwk"]["pub"] = std::move(jwk.second);
	info["contact"] = std::move(object["contact"]);
	info["createdAt"] = std::move(object["createdAt"]);

	extra_account_info::from_json(info).save(root_path_, kid);

	if (ah) {
		ah(std::move(kid));
	}
}

void daemon::operator()(const event_base &ev)
{
	fz::dispatch<
		acme::client::terms_of_service_event,
		acme::client::account_event,
		acme::client::error_event,
		acme::client::certificate_event
	>(ev, this,
		&daemon::on_terms,
		&daemon::on_account,
		&daemon::on_error,
		&daemon::on_certificate
	);
}

void daemon::try_to_renew_expiring_certs()
{
	fz::scoped_lock lock(mutex_);

	if (!how_to_serve_challenges_ || !root_path_.is_absolute())
		return;

	for (auto &c: workers_) {
		c.schedule_renewal();
	}
}

std::unique_ptr<client> daemon::make_new_client()
{
	return std::make_unique<client>(pool_, event_loop_, logger_, trust_store_);
}

void daemon::worker::schedule_renewal()
{
	auto certs = fz::load_certificates(info_.certs, tls_data_format::autodetect, true, &logger_);
	if (certs.empty()) {
		logger_.log_u(logmsg::error, "Couldn't load ACME certificate. Skipping it.");
		return;
	}

	auto exp = certs[0].get_expiration_time();
	auto act = certs[0].get_activation_time();

	if (!act.earlier_than(exp)) {
		logger_.log_u(logmsg::error, "Certificate activation date (%s) is not earlier than expiration date (%s). Skipping it.", act.get_rfc822(), exp.get_rfc822());
		return;
	}

	hostnames_ = get_hostnames(certs[0].get_alt_subject_names());

	auto interval = fz::duration::from_milliseconds((exp-act).get_milliseconds()/3*2);

	auto now = datetime::now();

	auto old_renewal_dt = renewal_dt_;

	if (error_count_) {
		logger_.log(logmsg::debug_warning, L"There have been %d consecutive errors so far while renewing the certificate. Using exponential backoff for the next renewal date.", error_count_);

		renewal_dt_ = renewal_dt_ + duration::from_minutes(std::min(
			  util::exp2_saturated<std::int64_t>(error_count_-1),
			  interval.get_minutes()
		));
	}
	else
	if (!renewal_dt_) {
		renewal_dt_ = certs[0].get_activation_time() + interval;
	}

	if (renewal_dt_ < now) {
		renewal_dt_ = now;
	}

	if (old_renewal_dt != renewal_dt_) {
		logger_.log(logmsg::status, L"Renewal will begin on the following date: %s.", renewal_dt_.get_rfc822());

		timer_id_ = stop_add_timer(timer_id_, renewal_dt_ - now, true);
	}
}

void daemon::worker::renew_now()
{
	logger_.log_u(logmsg::status, L"Starting renewal NOW.");

	if (!save_status()) {
		on_error(0, fzT("Could not save renewal status to file."), {});
		return;
	}

	extra_account_info extra = daemon_.load_extra_account_info(info_.acme()->account_id);
	if (!extra) {
		on_error(0, fzT("could not read or parse account info"), {});
	}

	client::opid_t id{};

	{
		scoped_lock lock(daemon_.mutex_);
		id = daemon_.client_->get_certificate(fz::uri(extra.directory), extra.jwk, hostnames_, info_.key, info_.key_password, daemon_.how_to_serve_challenges_, maximum_allowed_time_difference_from_acme_server, *this);
	}

	if (!id) {
		on_error(0, fzT("Could not execute acme::client::get_certificate: there's already an operation in progress."), {});
	}
}

void daemon::worker::load_status()
{
	using namespace fz::serialization;

	auto status_dir = daemon_.root_path_ / fzT("acme") / fzT("status");
	auto status_file = status_dir / to_native(id_of(info_.certs));

	xml_input_archive::file_loader l(status_file);
	xml_input_archive ar(l, xml_input_archive::options().verify_version(xml_input_archive::options::verify_mode::error));

	ar(
		nvp(error_count_, "error_count"),
		nvp(renewal_dt_, "renewal_dt")
	);

	if (!ar) {
		error_count_ = {};
		renewal_dt_ = {};
	}
	else {
		logger_.log(logmsg::debug_info, L"Loaded status from file `%s'.", status_file.str());
	}
}

bool daemon::worker::save_status()
{
	using namespace fz::serialization;

	auto status_dir = daemon_.root_path_ / fzT("acme") / fzT("status");

	if (!status_dir.mkdir(true, mkdir_permissions::cur_user_and_admins)) {
		logger_.log_u(logmsg::error, L"Could not create the directory for the certificates renewal status `%s'.", status_dir.str());
		return false;
	}

	auto status_file = status_dir / to_native(id_of(info_.certs));

	bool result = false;

	{
		xml_output_archive::file_saver s(status_file);
		xml_output_archive ar(s, xml_output_archive::options().save_result(&result));

		ar(
			nvp(error_count_, "error_count"),
			nvp(renewal_dt_, "renewal_dt")
		);
	}

	return result;
}

void daemon::worker::trash_status()
{
	auto status_dir = daemon_.root_path_ / fzT("acme") / fzT("status");
	auto trash_dir = status_dir / fzT("trash");
	auto status_file = to_native(id_of(info_.certs));

	auto status_path = status_dir / status_file;
	auto trash_path = trash_dir / status_file;

	if (status_path && trash_path && trash_dir.mkdir(true, mkdir_permissions::cur_user_and_admins)) {
		fz::rename_file(status_path, trash_path);
	}
}

void daemon::worker::remove_status()
{
	auto status_dir = daemon_.root_path_ / fzT("acme") / fzT("status");
	auto status_file = to_native(id_of(info_.certs));
	auto status_path = status_dir / status_file;

	fz::remove_file(status_path, false);
}

void daemon::empty_status_trash()
{
	auto status_dir = root_path_ / fzT("acme") / fzT("status");
	auto trash_dir = status_dir / fzT("trash");

	if (trash_dir) {
		fz::recursive_remove().remove(trash_dir);
	}
}

void daemon::worker::restore_trashed_status()
{
	auto status_dir = daemon_.root_path_ / fzT("acme") / fzT("status");
	auto trash_dir = status_dir / fzT("trash");
	auto status_file = to_native(id_of(info_.certs));

	auto status_path = status_dir / status_file;
	auto trash_path = trash_dir / status_file;

	if (trash_path && status_path) {
		fz::rename_file(trash_path, status_path);
	}
}

}
