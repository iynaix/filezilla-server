#include <libfilezilla/logger.hpp>
#include <libfilezilla/tls_info.hpp>
#include <libfilezilla/recursive_remove.hpp>
#include <libfilezilla/hash.hpp>
#include <libfilezilla/util.hpp>

#include "util/filesystem.hpp"
#include "util/io.hpp"
#include "util/overload.hpp"
#include "util/parser.hpp"

#include "securable_socket.hpp"

namespace fz {

securable_socket::securable_socket(event_loop &loop,  event_handler *event_handler, std::unique_ptr<socket> socket, logger_interface &logger)
	: socket_stack_interface(socket->root())
	, event_loop_(loop)
	, event_handler_(event_handler)
	, logger_(logger)
	, socket_stack_(new socket_stack(std::move(socket)))
{
}

securable_socket::securable_socket(event_loop &loop, event_handler *event_handler, std::unique_ptr<socket_stack> socket_stack, logger_interface &logger)
	: socket_stack_interface(socket_stack->root())
	, event_loop_(loop)
	, event_handler_(event_handler)
	, logger_(logger)
	, socket_stack_(std::move(socket_stack))
{
}

securable_socket::~securable_socket()
{
}

securable_socket_state securable_socket::get_securable_state() const
{
	if (tls_layer_) {
		auto s = tls_layer_->get_state();

		if (s == socket_state::connected)
			return securable_socket_state::secured;

		if (s != socket_state::none && s != socket_state::connecting)
			return securable_socket_state::invalid_socket_state;
	}

	return securable_state_;
}

std::optional<securable_socket::session_info> securable_socket::get_session_info() const
{
	if (!is_secure())
		return {};

	return securable_socket::session_info{
		tls_layer_->get_cipher(),
		tls_layer_->get_mac(),
		tls_layer_->get_protocol(),
		tls_layer_->get_key_exchange(),
		static_cast<tls_session_info::algorithm_warnings_t>(tls_layer_->get_algorithm_warnings())
	};
}

void securable_socket::set_verification_result(bool trusted)
{
	if (tls_layer_)
		tls_layer_->set_verification_result(trusted);
}

int securable_socket::shutdown()
{
	return socket_stack_->shutdown();
}

int securable_socket::shutdown_read()
{
	return socket_stack_->shutdown_read();
}

bool securable_socket::cert_info::set_root_path(const util::fs::native_path &root_path, logger_interface *logger)
{
	if (!root_path.is_absolute()) {
		if (logger)
			logger->log_u(logmsg::error, L"set_root_path: path \"%s\" is not absolute.", root_path);
		return false;
	}

	root_path_ = root_path;
	return resolve_paths(logger);
}

util::fs::native_path securable_socket::cert_info::key_path() const
{
	if (auto p = resolved_key_.filepath()) {
		return p->value;
	}

	return {};
}

util::fs::native_path securable_socket::cert_info::certs_path() const
{
	if (auto p = resolved_certs_.filepath()) {
		return p->value;
	}

	return {};
}

fz::const_tls_param_ref securable_socket::cert_info::key() const
{
	return resolved_key_;
}

fz::const_tls_param_ref securable_socket::cert_info::certs() const
{
	return resolved_certs_;
}

fz::native_string securable_socket::cert_info::key_password() const
{
	if (auto d = omni()) {
		return d->key_password;
	}

	return {};
}

std::vector<x509_certificate> securable_socket::cert_info::load_certs(logger_interface *logger) const
{
	if (!*this) {
		return {};
	}

	return fz::load_certificates(resolved_certs_, tls_data_format::autodetect, true, logger);
}


securable_socket::cert_info::extra securable_socket::cert_info::load_extra(fz::logger_interface *logger) const
{
	static auto get_hostnames = [](const auto &alt_subjects) {
		std::vector<std::string> hostnames;
		hostnames.reserve(alt_subjects.size());

		for (const auto &s: alt_subjects) {
			if (s.is_dns) {
				hostnames.push_back(s.name);
			}
		}

		return hostnames;
	};

	if (omni()) {
		if (auto && certs = load_certs(logger); !certs.empty()) {
			auto activation_time = certs[0].get_activation_time();
			auto expiration_time = certs[0].get_expiration_time();

			return omni_cert_info::extra{ certs[0].get_fingerprint_sha256(), certs[0].get_subject(), get_hostnames(certs[0].get_alt_subject_names()), activation_time, expiration_time };
		}
	}

	return {};
}

void securable_socket::cert_info::dump(logger_interface &logger, bool only_sha256)
{
	if (auto && certs = load_certs(&logger); !certs.empty()) {
		if (!only_sha256)
			logger.log_u(fz::logmsg::status, L"SHA1 certificate fingerprint: %s", certs[0].get_fingerprint_sha1());
		logger.log_u(fz::logmsg::status, L"SHA256 certificate fingerprint: %s", certs[0].get_fingerprint_sha256());

		return;
	}

	logger.log_u(fz::logmsg::status, L"No available certificate.");
}

securable_socket::cert_info securable_socket::cert_info::generate_selfsigned(tls_param key, const util::fs::native_path &root_path, logger_interface &logger, native_string const& password, std::string const& distinguished_name, std::vector<std::string> const& hostnames)
{
	logger.log_raw(logmsg::status, L"Generating self-signed certificate.");

	tls_param cert;

	if (key != tls_param()) {
		cert = tls_blob(tls_layer::generate_selfsigned_certificate(key, password, distinguished_name.empty() ? "CN=filezilla-server self signed certificate" : distinguished_name, hostnames, tls_layer::cert_type::any, logger));
	}
	else {
		auto [key_s, cert_s] = tls_layer::generate_selfsigned_certificate(password, distinguished_name.empty() ? "CN=filezilla-server self signed certificate" : distinguished_name, hostnames, tls_layer::cert_type::any, true, logger);

		key = tls_blob(std::move(key_s));
		cert = tls_blob(std::move(cert_s));
	}

	if (key && cert) {
		cert_info info = omni_cert_info {
			std::move(cert),
			std::move(key),
			password,
			omni_cert_info::sources::autogenerated{}
		};

		if (info.set_root_path(root_path, &logger)) {
			return info;
		}
	}

	return {};
}

securable_socket::cert_info securable_socket::cert_info::generate_exported() const
{
	if (root_path_.str().empty())
		return {};

	// We only support omni now.
	auto o = omni();
	if (!o) {
		return {};
	}

	omni_cert_info exported = *o;

	auto maybe_blobify = [this](tls_param p) -> tls_param {
		static auto is_ancestor = [](const util::fs::native_path &maybe_ancestor, const util::fs::native_path &maybe_descendant) {
			const auto &a = maybe_ancestor.str();
			const auto &d = maybe_descendant.str();

			return starts_with(d, a) && d.size() > a.size()+1 && util::fs::native_path::is_separator(d[a.size()]);
		};

		if (auto f = p.filepath(); f && is_ancestor(root_path_, fz::util::fs::native_path(f->value))) {
			return tls_blob(fz::util::io::read(f->value).to_view());
		}

		return p;
	};

	exported.key = maybe_blobify(resolved_key_);
	exported.certs = maybe_blobify(resolved_certs_);

	// Split key and certs, in case the blobs are the same
	if (auto k = exported.key.blob()) {
		if (auto c = exported.certs.blob()) {
			if (is_pem(k->value) && k->value == c->value) {
				std::tie(k->value, c->value) = split_key_and_cert(k->value);
			}
		}
	}

	if (!exported.key && !exported.certs) {
		return {};
	}

	return exported;
}


void securable_socket::cert_info::remove()
{
	fz::remove_file(key_path(), false);
	fz::remove_file(certs_path(), false);
}

std::string securable_socket::cert_info::fingerprint(const extra &extra) const
{
	if (auto e = extra.omni(); e && omni()) {
		return e->fingerprint;
	}

	return {};
}

static fz::tls_param resolve(fz::tls_param p, const fz::util::fs::native_path &r)
{
	if (auto f = p.filepath()) {
		f->value = r / f->value;
	}

	return p;
}

bool securable_socket::cert_info::resolve_paths(logger_interface *logger)
{
	auto error = [logger](auto &&... args) {
		if (logger)
			logger->log_u(logmsg::error, std::forward<decltype(args)>(args)...);

		return false;
	};

	auto info = [logger](auto &&... args) {
		if (logger)
			logger->log_u(logmsg::debug_info, std::forward<decltype(args)>(args)...);
	};

	if (!is_valid()) {
		return error(L"resolve_paths: cert_info is not valid.");
	}

	if (root_path_.str().empty()) {
		return error(L"resolve_paths: root path is empty.");
	}

	// The new, all-encompassing type
	if (auto d = omni()) {
		resolved_key_ = resolve(d->key, root_path_);
		resolved_certs_ = resolve(d->certs, root_path_);

		return true;
	}

	// Transform the exported type into one of the other, proper types.
	if (auto d = exported()) {
		auto certs = std::move(d->certs);
		auto key = std::move(d->key);

		if (certs.empty())
			return error("resolve_path: exported_cert_info: certs field is empty");

		cert_info info;

		if (auto *o = d->acme())
			info = std::move(*o);
		else
		if (auto *o = d->autogenerated())
			info = std::move(*o);
		else {
			auto imported_dir = root_path_ / fzT("imported");

			auto try_to_create = [&imported_dir](const native_string &name) -> native_string {
				std::size_t num_tries = 5;

				while (num_tries--) {
					auto now = datetime::now();
					auto date = now.format(fzT("%Y-%m-%dT%H.%M.%S"), datetime::utc);
					auto file_name = sprintf(fzT("%s-%s.%03dT.pem"), name, date, now.get_milliseconds());

					const auto &path = imported_dir / file_name;
					if (path.open(file::writing, file::current_user_and_admins_only | file::empty))
						return path;

					fz::sleep(fz::duration::from_milliseconds(10));
				}

				return {};
			};

			const auto &certs_path = try_to_create(fzT("certs"));
			if (certs_path.empty())
				return error(L"resolve_path: exported_cert_info: could not create certs file.");

			const auto &key_path = key.empty() ? certs_path : try_to_create(fzT("key"));
			if (key_path.empty()) {
				remove_file(certs_path, false);
				return error(L"resolve_path: exported_cert_info: could not create key file.");
			}

			if (auto *o = d->uploaded()) {
				uploaded_cert_info u = {
					std::move(key_path),
					std::move(certs_path),
					{}
				};

				u.password = std::move(o->password);

				info = std::move(u);
			}
			else {
				user_provided_cert_info u = {
					std::move(key_path),
					std::move(certs_path),
					{}
				};

				if (auto *o = d->user_provided())
					u.password = std::move(o->password);

				info = std::move(u);
			}
		}

		if (!info.set_root_path(root_path_, logger))
			return false;

		if (
			   util::io::write(info.certs_path().open(fz::file::writing, fz::file::current_user_and_admins_only | fz::file::empty), certs)
			&& (key.empty() || util::io::write(info.key_path().open(fz::file::writing, fz::file::current_user_and_admins_only | fz::file::empty), key))
		) {
			*this = std::move(info);
		}
		else {
			fz::remove_file(info.certs_path(), false);
			fz::remove_file(info.key_path(), false);

			return error(L"resolve_path: exported_cert_info: could not write to certs and/or key file.");
		}

		return true;
	}

	// We do not support the "old" types anymore, we transform them into the new "omni" type.
	if (auto d = user_provided()) {
		info(L"Converting the user_provided cert type (%d) into the omni type (%d)", index(), variant(omni_cert_info()).index());

		static_cast<variant&>(*this) = omni_cert_info{
			fz::tls_filepath(root_path_ / std::move(d->key_path)),
			fz::tls_filepath(root_path_ / std::move(d->certs_path)),
			std::move(d->password),

			omni_cert_info::sources::provided{}
		};

		return resolve_paths(logger);
	}

	if (auto d = uploaded()) {
		info(L"Converting the uploaded cert type (%d) into the omni type (%d)", index(), variant(omni_cert_info()).index());

		static_cast<variant&>(*this) = omni_cert_info{
			fz::tls_filepath(root_path_ / std::move(d->certs_path)),
			fz::tls_filepath(root_path_ / std::move(d->key_path)),
			std::move(d->password),

			omni_cert_info::sources::provided{}
		};

		return resolve_paths(logger);
	}

	if (auto d = autogenerated()) {
		info(L"Converting the autogenerated cert type (%d) into the omni type (%d)", index(), variant(omni_cert_info()).index());

		fz::util::fs::native_path dir = fz::to_native(fz::replaced_substrings(d->fingerprint, ":", ""));

		static_cast<variant&>(*this) = omni_cert_info{
			fz::tls_filepath(root_path_ / dir / fzT("cert.pem")),
			fz::tls_filepath(root_path_ / dir / fzT("key.pem")),
			{},
			omni_cert_info::sources::autogenerated{}
		};

		return resolve_paths(logger);
	}

	if (auto d = acme()) {
		info(L"Converting the acme cert type (%d) into the omni type (%d)", index(), variant(omni_cert_info()).index());

		std::sort(d->hostnames.begin(), d->hostnames.end());
		d->hostnames.erase(std::unique(d->hostnames.begin(), d->hostnames.end()), d->hostnames.end());

		auto hashed_hosts = [&d] {
			fz::hash_accumulator acc(hash_algorithm::md5);

			for (auto &h: d->hostnames)
				acc.update(h);

			return acc.digest();
		}();

		auto hashed_account_id = fz::md5(d->account_id);
		auto encoded_account_id = fz::base32_encode(hashed_account_id, base32_type::locale_safe, false);
		auto encoded_hosts = fz::base32_encode(hashed_hosts, base32_type::locale_safe, false);

		auto dir = fz::util::fs::native_path(fzT("acme")) / fz::to_native(encoded_account_id) / fz::to_native(encoded_hosts);

		static_cast<variant&>(*this) = omni_cert_info{
			fz::tls_filepath(root_path_ / dir / fzT("cert.pem")),
			fz::tls_filepath(root_path_ / dir / fzT("key.pem")),
			{},
			omni_cert_info::sources::acme{
				std::move(d->account_id),
				d->autorenew
			}
		};

		return resolve_paths(logger);
	}

	error(L"resolve_path: unhandled certificate type %d.", index());
	return false;
}

securable_socket::securer::securer(securable_socket &owner,
								   bool make_server, tls_ver min_tls_ver,
								   const securable_socket::cert_info *cert_info,
								   tls_system_trust_store *trust_store,
								   securable_socket *socket_to_get_tls_session_from,
								   std::string_view preamble,
								   std::vector<std::string> alpns,
								   bool alpn_mandatory)
	: owner_(owner)
	, make_server_(make_server)
	, socket_to_get_tls_session_from_(socket_to_get_tls_session_from)
	, preamble_(preamble)
	, alpns_(std::move(alpns))
	, alpn_mandatory_(alpn_mandatory)
{
	owner_.logger_.log_u(logmsg::debug_debug, L"securer(%d) ENTERING state = %d", make_server, owner_.securable_state_);

	if (owner_.tls_layer_ != nullptr) {
		auto s = owner_.tls_layer_->get_state();

		if (s == socket_state::connected) {
			if (socket_to_get_tls_session_from_ && !owner_.tls_layer_->resumed_session()) {
				owner_.securable_state_ = securable_socket_state::session_not_resumed;
			}
			else
			if (alpn_mandatory_ && owner_.tls_layer_->get_alpn().empty()) {
				owner_.securable_state_ = securable_socket_state::wrong_alpn;
			}
			else {
				owner_.securable_state_ = securable_socket_state::secured;
			}
		}
		else
		if (s != socket_state::none && s != socket_state::connecting)
			owner_.securable_state_ = securable_socket_state::invalid_socket_state;
	}
	else
	if (owner_.securable_state_ == securable_socket_state::insecure) {
		if (socket_to_get_tls_session_from && !socket_to_get_tls_session_from->is_secure()) {
			owner_.securable_state_ = securable_socket_state::session_socket_not_secure;
		}
		else {
			owner_.tls_layer_ = new fz::tls_layer(owner_.event_loop_, owner_.event_handler_, owner_.socket_stack_->top(), trust_store, owner_.logger_);
			owner_.tls_layer_->set_min_tls_ver(min_tls_ver);
			owner_.tls_layer_->set_unexpected_eof_cb(owner_.eof_cb_);

			owner_.securable_state_ = securable_socket_state::about_to_secure;

			if (cert_info) {
				owner_.logger_.log_u(logmsg::debug_debug, L"calling tls_layer_->set_key_and_certs(<%s>, <%s>, \"****\")",
														  cert_info->key().url(), cert_info->certs().url());

				if (!owner_.tls_layer_->set_key_and_certs(cert_info->key(), cert_info->certs(), cert_info->key_password())) {
					owner_.securable_state_ = securable_socket_state::failed_setting_certificate_file;
					delete owner_.tls_layer_;
					owner_.tls_layer_ = nullptr;
					owner_.socket_stack_->set_event_handler(owner_.event_handler_);
				}
			}
		}
	}

	owner_.logger_.log_u(logmsg::debug_debug, L"securer(%d) EXITING state = %d", make_server, owner_.securable_state_);
}

securable_socket::securer::~securer()
{
	owner_.logger_.log_u(logmsg::debug_debug, L"~securer(%d) ENTERING state = %d", make_server_, owner_.securable_state_);

	if (owner_.securable_state_ == securable_socket_state::about_to_secure) {
		owner_.securable_state_ = securable_socket_state::securing;

		owner_.socket_stack_->push(std::unique_ptr<fz::tls_layer>(owner_.tls_layer_));

		auto get_session_parameters = [this] {
			if (socket_to_get_tls_session_from_)
				return socket_to_get_tls_session_from_->tls_layer_->get_session_parameters();
			return std::vector<uint8_t>{};
		};

		auto get_host_name = [this] {
			if (socket_to_get_tls_session_from_)
				return socket_to_get_tls_session_from_->tls_layer_->get_hostname();
			return fz::native_string{};
		};

		bool success = true;

		if (!alpns_.empty()) {
			owner_.logger_.log_u(logmsg::debug_debug, L"calling tls_layer_->set_alpn()");
			success = owner_.tls_layer_->set_alpn(alpns_, make_server_);
		}

		if (success) {
			if (make_server_)
				success = owner_.tls_layer_->server_handshake(get_session_parameters(), preamble_, fz::tls_server_flags::no_auto_ticket);
			else
				success = owner_.tls_layer_->client_handshake(owner_.event_handler_, get_session_parameters(), get_host_name());
		}

		if (!success && owner_.event_handler_)
			owner_.event_handler_->send_event<socket_event>(owner_.tls_layer_, socket_event_flag::connection, EPROTO);
	}

	owner_.logger_.log_u(logmsg::debug_debug, L"~securer(%d) EXITING state = %d", make_server_, owner_.securable_state_);
}

securable_socket::securer securable_socket::make_secure_server(tls_ver min_tls_ver, const cert_info &cert_info, securable_socket *socket_to_get_tls_session_from, std::string_view preamble, std::vector<std::string> alpns, bool alpn_mandatory)
{
	return securer(*this, true, min_tls_ver, &cert_info, nullptr, socket_to_get_tls_session_from, preamble, std::move(alpns), alpn_mandatory);
}

securable_socket::securer securable_socket::make_secure_client(tls_ver min_tls_ver, const cert_info *cert_info, tls_system_trust_store *trust_store, securable_socket *socket_to_get_tls_session_from, std::vector<std::string> alpns, bool alpn_mandatory)
{
	return securer(*this, false, min_tls_ver, cert_info, trust_store, socket_to_get_tls_session_from, {}, std::move(alpns), alpn_mandatory);
}

int securable_socket::new_session_ticket()
{
	if (!tls_layer_) {
		return EINVAL;
	}
	return tls_layer_->new_session_ticket();
}

std::string securable_socket::get_alpn() const
{
	if (get_securable_state() == securable_socket_state::secured)
		return tls_layer_->get_alpn();

	return {};
}

void securable_socket::set_unexpected_eof_cb(std::function<bool ()> cb)
{
	eof_cb_= std::move(cb);
}

bool securable_socket::cert_info::extra::expired() const
{
	return std::visit(util::overload {
		[](const std::monostate &) {
			return false;
		},
		[](const auto &e) {
			return e.expiration_time < datetime::now();
		}
					  }, static_cast<const variant&>(*this));
}

std::pair<std::string, std::string> split_key_and_cert(std::string_view data)
{
	using namespace std::string_view_literals;

	std::pair<std::string, std::string> ret;

	std::size_t begin_pos = data.npos;
	bool parsing_certificate = false;

	for (const auto t: fz::strtokenizer(data, "\n\r", true))
	{
		if (begin_pos == data.npos) {
			if (starts_with(t, "-----BEGIN "sv)) {
				if (ends_with(t, "PRIVATE KEY-----"sv)) {
					begin_pos = std::size_t(t.data() - data.data());
					parsing_certificate = false;
				}
				else
				if (ends_with(t, "CERTIFICATE-----"sv)) {
					begin_pos = std::size_t(t.data() - data.data());
					parsing_certificate = true;
				}
			}
		}

		if (begin_pos != data.npos) {
			if (starts_with(t, "-----END "sv)) {
				if (!parsing_certificate && ends_with(t, "PRIVATE KEY-----"sv)) {
					auto end_pos = std::size_t(t.data() - data.data()) + t.size();
					ret.first = data.substr(begin_pos, end_pos-begin_pos);
					begin_pos = data.npos;
				}
				else
				if (parsing_certificate && ends_with(t, "CERTIFICATE-----"sv)) {
					auto end_pos = std::size_t(t.data() - data.data()) + t.size();
					ret.second = data.substr(begin_pos, end_pos-begin_pos);
					begin_pos = data.npos;
				}
			}
		}
	}

	return ret;
}

std::vector<std::string> get_hostnames(const std::vector<x509_certificate::subject_name> &subjects)
{
	std::vector<std::string> hostnames;
	hostnames.reserve(subjects.size());

	for (const auto &s: subjects)
		if (s.is_dns)
			hostnames.push_back(s.name);

	return hostnames;
}

std::vector<std::string> get_hostnames(const_tls_param_ref certs, logger_interface *logger)
{
	auto list = load_certificates(certs, tls_data_format::autodetect, true, logger);
	if (!list.empty()) {
		return get_hostnames(list[0].get_alt_subject_names());
	}

	return {};
}

blob_obfuscator::blob_obfuscator()
	: key_(symmetric_key::generate())
{}

static const std::string_view obfuscated_blob_scheme_ = "blob:obfuscated:";

static bool get_obfuscated_parts(std::string_view blob, std::string_view &md5, std::string_view &encrypted)
{
	util::parseable_range r(blob);

	return match_string(r, obfuscated_blob_scheme_) && parse_until_lit(r, md5, {':'}) && md5.size() == 32 && lit(r, ':') && parse_until_eol(r, encrypted);
}

std::string_view blob_obfuscator::get_obfuscated_blob_id(std::string_view blob)
{
	std::string_view md5;
	std::string_view encrypted;

	if (get_obfuscated_parts(blob, md5, encrypted)) {
		return md5;
	}

	return {};
}

std::string blob_obfuscator::get_obfuscated_blob_id(std::string &&blob)
{
	return std::string(get_obfuscated_blob_id(blob));
}

bool blob_obfuscator::is_obfuscated(std::string_view blob)
{
	return !get_obfuscated_blob_id(blob).empty();
}

bool blob_obfuscator::is_obfuscated(const tls_param &param)
{
	auto b = param.blob();
	if (!b) {
		return false;
	}

	return is_obfuscated(b->value);
}

bool blob_obfuscator::is_obfuscated(const securable_socket::cert_info &info)
{
	auto o = info.omni();
	if (!o) {
		return false;
	}

	return is_obfuscated(o->key);
}

std::pair<const std::string *, std::string_view> blob_obfuscator::get_obfuscated_blob(const securable_socket::cert_info &info)
{
	auto o = info.omni();
	if (!o) {
		return {};
	}

	auto b = o->key.blob();
	if (!b) {
		return {};
	}

	auto id = get_obfuscated_blob_id(b->value);
	if (!id.empty()) {
		return {&b->value, id};
	}

	return {};
}

bool blob_obfuscator::obfuscate(securable_socket::cert_info &info)
{
	auto o = info.omni();
	if (!o) {
		return false;
	}

	auto b = o->key.blob();
	if (!b) {
		return false;
	}

	if (is_obfuscated(b->value)) {
		return false;
	}

	auto obfuscated = encrypt(b->value, key_);
	auto id = hex_encode<std::string>(md5(b->value));

	b->value.assign(obfuscated_blob_scheme_.begin(), obfuscated_blob_scheme_.end());
	b->value.append(id.begin(), id.end());
	b->value.append(1, ':');
	b->value.append(obfuscated.begin(), obfuscated.end());

	return true;
}

bool blob_obfuscator::deobfuscate(std::string &blob)
{
	std::string_view md5;
	std::string_view encrypted;

	if (!get_obfuscated_parts(blob, md5, encrypted)) {
		return false;
	}

	auto res = decrypt(encrypted, key_);
	if (res.empty()) {
		return false;
	}

	blob.assign(res.begin(), res.end());

	return true;
}

bool blob_obfuscator::deobfuscate(tls_param &param)
{
	auto b = param.blob();
	if (!b) {
		return false;
	}

	return deobfuscate(b->value);
}

bool blob_obfuscator::deobfuscate(fz::securable_socket::cert_info &info)
{
	auto o = info.omni();
	if (!o) {
		return false;
	}

	return deobfuscate(o->key);
}

}
