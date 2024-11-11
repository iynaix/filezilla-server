#include <algorithm>
#include <unordered_set>

#include <libfilezilla/util.hpp>

#include "file_based_authenticator.hpp"
#include "error.hpp"

#include "../serialization/archives/xml.hpp"
#include "../serialization/types/binary_address_list.hpp"
#include "../impersonator/client.hpp"
#include "../remove_event.hpp"
#include "../logger/type.hpp"

namespace fz::authentication {

namespace {
	using xml_archiver = util::xml_archiver<authentication::file_based_authenticator::groups, authentication::file_based_authenticator::users>;
}

class file_based_authenticator::worker
{
public:
	class operation;

	worker(file_based_authenticator &owner, std::string_view name, address_type family, std::string_view ip, event_handler *target, logger::modularized::meta_map meta_for_logging)
		: name_(name)
		, family_(family)
		, ip_(ip)
		, target_(target)
		, owner_(owner)
		, logger_(owner.logger_, {}, std::move(meta_for_logging))
	{
	}

private:
	friend file_based_authenticator;

	void authenticate(const methods_list &methods, available_methods &&available_methods);
	void remove()
	{
		fz::scoped_lock lock(owner_.mutex_);
		owner_.workers_->erase(self_in_workers_);
	}

	std::string name_{};
	address_type family_{};
	std::string ip_{};
	event_handler *target_{};
	file_based_authenticator &owner_;
	logger::modularized logger_;

	impersonation_token impersonation_token_;

	workers::iterator self_in_workers_;
};

class file_based_authenticator::worker::operation: public authenticator::operation
{
public:
	shared_user get_user() override;
	available_methods get_methods() override;
	error get_error() override;

	bool next(const methods_list &methods) override;
	void stop() override;

	operation(worker &w, shared_user &&su, available_methods &&m, error e);

private:
	friend worker;

	worker *worker_;
	shared_user shared_user_;
	available_methods methods_;
	error error_;
};

shared_user file_based_authenticator::worker::operation::get_user()
{
	if (!error_ &&  !methods_.is_auth_necessary())
		return shared_user_;

	return {};
}

available_methods file_based_authenticator::worker::operation::get_methods()
{
	return methods_;
}

error file_based_authenticator::worker::operation::get_error()
{
	return error_;
}

bool file_based_authenticator::worker::operation::next(const methods_list &methods)
{
	if (worker_) {
		worker_->authenticate(methods, std::move(methods_));

		worker_ = nullptr;

		return true;
	}

	return false;
}

void file_based_authenticator::worker::operation::stop()
{
	if (worker_) {
		worker_->logger_.log_u(logmsg::debug_debug, L"operation %p stop() erasing worker %p", this, worker_);

		worker_->remove();
		worker_ = nullptr;
	}
}

file_based_authenticator::worker::operation::operation(worker &w, shared_user &&su, available_methods &&m, error e)
	: worker_(&w)
	, shared_user_(std::move(su))
	, methods_(std::move(m))
	, error_(std::move(e))
{
	if (w.logger_.should_log(logmsg::debug_debug))
		w.logger_.log_u(logmsg::debug_debug, L"Worker %p created new operation %p, with shared_user = %p, methods = [%s], error = %d", &w, this, shared_user_.get(), methods_, int(error_));
}

file_based_authenticator::file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string impersonator_exe)
	: thread_pool_(thread_pool)
	, event_loop_(event_loop)
	, logger_(logger, "File-based Authenticator")
	, rlm_(rlm)
	, workers_(std::make_unique<workers>())
	, impersonator_exe_(std::move(impersonator_exe))
{
}

file_based_authenticator::file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string groups_path, native_string users_path, native_string impersonator_exe)
	: file_based_authenticator(thread_pool, event_loop, logger, rlm, impersonator_exe)
{
	auto a = std::make_unique<xml_archiver>(rlm.event_loop_);
	a->set_values(
		{groups_, { "", groups_path }},
		{users_, { "", users_path }}
	);

	xml_archiver_ = std::move(a);

	load();
}


file_based_authenticator::file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string groups_path, fz::authentication::file_based_authenticator::groups &&groups, native_string users_path, fz::authentication::file_based_authenticator::users &&users, native_string impersonator_exe)
	: file_based_authenticator(thread_pool, event_loop, logger, rlm, impersonator_exe)
{
	auto a = std::make_unique<xml_archiver>(rlm.event_loop_, fz::duration::from_milliseconds(100), &mutex_);

	a->set_values(
		{groups_, { "", groups_path }},
		{users_, { "", users_path }}
	);

	xml_archiver_ = std::move(a);

	set_groups_and_users(std::move(groups), std::move(users));
}

file_based_authenticator::~file_based_authenticator()
{
}

serialization::xml_input_archive::error_t file_based_authenticator::load_into(fz::authentication::file_based_authenticator::groups &groups, fz::authentication::file_based_authenticator::users &users)
{
	if (xml_archiver *a = static_cast<xml_archiver *>(xml_archiver_.get())) {
		return a->load_into(groups, users);
	}

	return { EINVAL, "file_based_authenticator was constructed without paths to the users and groups files." };
}

bool file_based_authenticator::load()
{
	groups groups;
	users users;

	if (load_into(groups, users) == 0) {
		set_groups_and_users(std::move(groups), std::move(users));
		return true;
	}

	return false;
}

bool file_based_authenticator::save(util::xml_archiver_base::event_dispatch_mode mode)
{
	if (xml_archiver_) {
		return xml_archiver_->save_now(mode) == 0;
	}

	return false;
}

void file_based_authenticator::set_save_result_event_handler(event_handler *handler)
{
	if (xml_archiver_) {
		xml_archiver_->set_event_handler(handler);
	}
}

void file_based_authenticator::save_later()
{
	if (xml_archiver_) {
		xml_archiver_->save_later();
	}
}

bool file_based_authenticator::save(const native_string &groups_path, const groups &groups, const native_string &users_path, const users &users)
{
	return xml_archiver::save_now(
		{groups, { "", groups_path }},
		{users, { "", users_path }}
	) == 0;
}

void file_based_authenticator::sanitize(file_based_authenticator::groups &groups, file_based_authenticator::users &users, logger_interface *logger)
{
	// Sanitize groups
	for (auto it = groups.begin(); it != groups.end();) {
		auto &g = *it;

		//Disallow invalid chars in group names
		if (g.first.empty() || g.first.find_first_of(groups.invalid_chars_in_name) != std::string::npos) {
			if (logger)
				logger->log_u(logmsg::error, L"Group has invalid name \"%s\", removing it from the list", g.first);

			it = groups.erase(it);
			continue;
		}

		it = ++it;
	}

	static const user_entry &system_user_entry = [] () -> user_entry & {
		static user_entry u;
		u.credentials.password.impersonate();
		u.enabled = false;
		u.description = "This user can impersonate any system user.";
		u.mount_table = { { "/", fzT("%<home>"), fz::tvfs::mount_point::read_write } };

		return u;
	}();

	auto &system_user = *users.try_emplace(users.system_user_name, system_user_entry).first;

	// Sanitize users
	for (auto it = users.begin(); it != users.end();) {
		auto &u = *it;

		// Sanitize system user
		bool is_system_user = &u == &system_user;
		if (is_system_user && !u.second.credentials.password.get_impersonation()) {
			if (logger)
				logger->log_u(logmsg::warning, L"%s doesn't have impersonation set. Forcing credentials to 'impersonation'.", users::system_user_name);

			u.second.credentials.password.impersonate();
		}

		//Disallow invalid chars in usernames
		if ((!is_system_user && (u.first.empty() || u.first.find_first_of(users.invalid_chars_in_name) != std::string::npos))) {
			if (logger)
				logger->log_u(logmsg::error, L"User has invalid name \"%s\", removing it from the list", u.first);

			it = users.erase(it);
			continue;
		}

		// remove references to non-existing or duplicated groups
		u.second.groups.erase(std::remove_if(u.second.groups.begin(), u.second.groups.end(), [&](auto &g) {
			auto already_seen = [set = std::unordered_set<std::string>{}](const std::string &s) mutable {
				auto [it, inserted] = set.insert(s);
				return !inserted;
			};

			bool group_doesnt_exist = groups.count(g) == 0;
			if (group_doesnt_exist && logger)
				logger->log_u(logmsg::warning, L"Group [%s] referenced by user [%s] does not exist or has been in a previous sanitizing step. Ignoring.", g, u.first);

			bool duplicated = already_seen(g);
			if (duplicated && logger)
				logger->log_u(logmsg::warning, L"Group [%s] is referenced multiple times by user [%s]. Ignoring the excess references", g, u.first);

			return group_doesnt_exist || duplicated;
		}), u.second.groups.end());

		if (!u.second.methods.is_auth_possible()) {
			u.second.methods = u.second.credentials.get_most_secure_methods();
			logger->log_u(logmsg::debug_info, L"User \"%s\" did not have any auth methods configured, defaulting to the most secure ones based on the available credentials: [%s].", u.first, u.second.methods);
		}
		else
		if (!u.second.credentials.is_valid_for(u.second.methods, logger)) {
			logger->log_u(logmsg::warning, L"User \"%s\" has auth methods [%s] that do not match the credentials. Login will not be possible.", u.first, u.second.methods);
		}

		it = ++it;
	}

	if (users.default_impersonator.index() != users::impersonator::any(users::impersonator::native()).index()) {
		if (logger)
			logger->log_u(logmsg::debug_warning, L"The type of the default impersonator is not valid on this platform. Resetting the default impersonator.");

		users.default_impersonator = users::impersonator::native();
	}
}

void file_based_authenticator::update()
{
	sanitize(groups_, users_, &logger_);

	for (auto l_it = group_limiters_.begin(); l_it != group_limiters_.end();) {
		if (auto g_it = groups_.find(l_it->first); g_it == groups_.end()) {
			l_it = group_limiters_.erase(l_it);
		}
		else {
			update_group_limiters(l_it->second, *g_it);
			++l_it;
		}
	}

	auto default_impersonator_token = users_.default_impersonator.get_token();

	for (auto wu_it = weak_users_map_.begin(); wu_it != weak_users_map_.end();) {
		if (auto su = wu_it->second.lock(); !su) {
			wu_it = weak_users_map_.erase(wu_it);
		}
		else {
			std::shared_ptr<tvfs::mount_tree> mt;
			std::shared_ptr<tvfs::backend> b;

			{
				auto locked_su = su->lock();
				auto u_it = users_.find(locked_su->id);

				static const auto has_filesystem_impersonator = [](const auto &u) {
					if (auto *i = u.credentials.password.get_impersonation())
						return !i->login_only;

					return false;
				};

				// If the user has been deleted
				// Or if it's been disabled
				// Or if it doesn't have a filesystem impersonator and the default impersonator token has changed
				if (u_it == users_.end() || !u_it->second.enabled || !(has_filesystem_impersonator(u_it->second) || (locked_su->get_impersonation_token() == default_impersonator_token))) {
					// Then signal that the shared user must be disposed of, to whomever else might be holding its pointer.
					// This will also make sessions log out if it's this user that was holding them open.
					locked_su->id.clear();
					wu_it = weak_users_map_.erase(wu_it);
				}
				else {
					update_shared_user(*locked_su, u_it->second);

					mt = locked_su->mount_tree;
					b = locked_su->impersonator;

					++wu_it;
				}
			}

			tvfs::async_autocreate_directories(std::move(mt), std::move(b), async_receive(async_handlers_.try_emplace(nullptr, event_loop_).first->second) >> [su = std::move(su)]() mutable {
					notify(su);
			});
		}
	}
}

void file_based_authenticator::set_groups_and_users(file_based_authenticator::groups &&groups, file_based_authenticator::users &&users)
{
	scoped_lock lock(mutex_);

	groups_ = std::move(groups);
	users_ = std::move(users);

	update();
}

void file_based_authenticator::get_groups_and_users(file_based_authenticator::groups &groups, file_based_authenticator::users &users)
{
	scoped_lock lock(mutex_);

	groups = groups_;
	users = users_;
}

bool file_based_authenticator::add_user(std::string name, user_entry data)
{
	scoped_lock lock(mutex_);

	if (users_.emplace(std::move(name), std::move(data)).second) {
		update();

		return true;
	}

	return false;
}

bool file_based_authenticator::remove_user(const std::string &name)
{
	scoped_lock lock(mutex_);

	if (users_.erase(name)) {
		update();

		return true;
	}

	return false;
}

std::pair<std::string, std::string> file_based_authenticator::make_temp_user(tvfs::mount_table mt)
{
	logger_.log_raw(logmsg::status, L"Creating a temporary user...");

	std::string name;
	std::string password = hex_encode<std::string>(random_bytes(16));

	user_entry ue;
	ue.mount_table = std::move(mt);
	ue.credentials.password = any_password(default_password(password));
	ue.methods = { authentication::method::password() };

	scoped_lock lock(mutex_);

	// The likelihood of a 128 bits random username to be already in the user maps is insignificant.
	// Nonetheless, let's adhere to the best practices of defensive programming and check for collisions.
	// It looks and feels silly, but... you never know.
	constexpr unsigned max_tries = 5;

	for (unsigned tries = 0; tries < max_tries; ++tries) {
		name = hex_encode<std::string>(random_bytes(16));

		if (users_.count(name) == 0 && temp_users_.try_emplace(name, std::move(ue)).second) {
			logger_.log_u(logmsg::status, L"Successfully created temporary user '%s'.", name);
			return {std::move(name), std::move(password)};
		}
	}

	logger_.log_raw(logmsg::error, L"Couldn't create the temporary user.");
	return {};
}

bool file_based_authenticator::remove_temp_user(const std::string &name)
{
	scoped_lock lock(mutex_);

	if (temp_users_.erase(name)) {
		logger_.log_u(logmsg::status, L"Succefully removed temporary user '%s'.", name);
		return true;
	}

	logger_.log_u(logmsg::status, L"Couldn't remove temporary user '%s'.", name);
	return false;
}

void file_based_authenticator::authenticate(std::string_view name, const methods_list &methods, address_type family, std::string_view ip, event_handler &target, logger::modularized::meta_map meta_for_logging)
{
	scoped_lock lock(mutex_);

	auto &worker = workers_->emplace_front(*this, name, family, ip, &target, std::move(meta_for_logging));
	worker.self_in_workers_ = workers_->begin();

	worker.authenticate(methods, {});
}

void file_based_authenticator::stop_ongoing_authentications(event_handler &target)
{
	scoped_lock lock(mutex_);

	async_handlers_.erase(&target);

	remove_events<operation::result_event>(&target, *this);

	workers_->remove_if([&](worker &w) {
		return w.target_ == &target;
	});
}

void file_based_authenticator::update_shared_user(user &user, const user_entry &entry)
{
	logger::modularized logger(logger_, {}, { { "user", user.name }});

	user.mount_tree = std::make_shared<tvfs::mount_tree>(entry.mount_table, tvfs::placeholders::map {
		{ tvfs::placeholders::user_name, fz::to_native(user.name) },
		{ tvfs::placeholders::home_dir, user.home_dir() },
		{ tvfs::placeholders::anything(fzT("%p")), fz::tvfs::placeholders::make_invalid_value(fzT("%%<%p> is not a recognized placeholder"))}
	}, logger);

	if (!user.limiter)
		user.limiter = std::make_shared<rate_limiter>(&rlm_);

	user.limiter->set_limits(entry.rate_limits.inbound, entry.rate_limits.outbound);

	user.session_inbound_limit = entry.rate_limits.session_inbound;
	user.session_outbound_limit = entry.rate_limits.session_outbound;

	user.session_open_limits.files = entry.session_open_limits.files;
	user.session_open_limits.directories = entry.session_open_limits.directories;

	user.session_count_limiter.set_limit(entry.session_count_limit);

	user.extra_limiters.clear();
	user.extra_limiters.reserve(entry.groups.size());

	user.extra_session_count_limiters.clear();
	user.extra_session_count_limiters.reserve(entry.groups.size());

	static auto update_limit = [](const auto &g, auto &u, auto unlimited) {
		if (g != unlimited) {
			if (u == unlimited || g < u)
				u = g;
		}
	};

	for (auto git = entry.groups.crbegin(), gend = entry.groups.crend(); git != gend; ++git) {
		auto g = groups_.find(*git);
		if (g != groups_.end()) {
			logger.insert_meta("group", g->first);

			user.mount_tree->merge_with(g->second.mount_table, logger);
			auto &gl = get_or_make_group_limiters(*g);
			user.extra_limiters.push_back(gl.shared_rate_limiter);
			user.extra_session_count_limiters.push_back(gl.session_count_limiter);

			update_limit(g->second.rate_limits.session_inbound, user.session_inbound_limit, rate::unlimited);
			update_limit(g->second.rate_limits.session_outbound, user.session_outbound_limit, rate::unlimited);

			update_limit(g->second.session_open_limits.files, user.session_open_limits.files, tvfs::open_limits::unlimited);
			update_limit(g->second.session_open_limits.directories, user.session_open_limits.directories, tvfs::open_limits::unlimited);
		}
	}

	logger.erase_meta("group");

	if (logger.should_log(logmsg::debug_info)) {
		logger.log_raw(logmsg::debug_info, L"Effective mount points:");
		user.mount_tree->dump("  > ", logger, logmsg::debug_info);
	}

	std::sort(user.extra_limiters.begin(), user.extra_limiters.end());
}

void file_based_authenticator::update_group_limiters(group_limiters &limiters, const file_based_authenticator::groups::value_type &g)
{
	limiters.shared_rate_limiter->set_limits(g.second.rate_limits.inbound, g.second.rate_limits.outbound);
	limiters.session_count_limiter->set_limit(g.second.session_count_limit);
}

shared_user file_based_authenticator::get_or_make_shared_user(const std::string &name, const user_entry &entry, bool is_from_system, impersonation_token &&token)
{
	auto &weak_user_in_map = weak_users_map_[name];
	auto shared_user_in_map = weak_user_in_map.lock();

	if (shared_user_in_map) {
		// Has the token changed from what we already have?
		if (auto &old_user = *shared_user_in_map->lock(); !(old_user.get_impersonation_token() == token)) {
			// If so, we ought to let the old user's holders know that they're done with it.
			old_user.id.clear();
			notify(shared_user_in_map);

			shared_user_in_map.reset();
		}
	}

	if (!shared_user_in_map) {
		authentication::user user(is_from_system ? users::system_user_name : name, name);

		if (token)
			user.impersonator = std::make_shared<impersonator::client>(thread_pool_, logger_, std::move(token), impersonator_exe_);

		update_shared_user(user, entry);

		weak_user_in_map = shared_user_in_map = make_shared_user<util::locking_wrapper<authentication::user, fz::mutex&>>(mutex_, std::move(user));
	}

	return shared_user_in_map;
}

file_based_authenticator::group_limiters &file_based_authenticator::get_or_make_group_limiters(const groups::value_type &g)
{
	auto it = group_limiters_.find(g.first);

	if (it == group_limiters_.end()) {
		bool inserted;

		std::tie(it, inserted) = group_limiters_.insert({g.first, {
			std::make_shared<rate_limiter>(&rlm_),
			std::make_shared<util::limited_copies_counter>(fz::sprintf("group «%s»", g.first))
		}});

		if (inserted) {
			update_group_limiters(it->second, g);
		}
	}

	return it->second;
}

impersonation_token file_based_authenticator::users::impersonator::msw::get_token() const
{
#if defined(FZ_WINDOWS)
	if (enabled)
		return impersonation_token(name, password);
#endif

	return {};
}

impersonation_token file_based_authenticator::users::impersonator::nix::get_token() const
{
#if !defined(FZ_WINDOWS)
	if (enabled)
		return impersonation_token(name, impersonation_flag::pwless, group);
#endif

	return {};
}

file_based_authenticator::users::impersonator::nix *file_based_authenticator::users::impersonator::any::nix()
{
	return std::get_if<impersonator::nix>(this);
}

file_based_authenticator::users::impersonator::msw *file_based_authenticator::users::impersonator::any::msw()
{
	return std::get_if<impersonator::msw>(this);
}

file_based_authenticator::users::impersonator::native *file_based_authenticator::users::impersonator::any::native()
{
	return std::get_if<impersonator::native>(this);
}

impersonation_token file_based_authenticator::users::impersonator::any::get_token() const
{
	return std::visit([](const auto &i) { return i.get_token(); }, static_cast<const variant &>(*this));
}


/******************************************************************/


void file_based_authenticator::worker::authenticate(const methods_list &methods, available_methods &&available_methods)
{
	error error{};

	scoped_lock lock(owner_.mutex_);

	if (logger_.should_log(logmsg::debug_debug))
		logger_.log_u(logmsg::debug_debug, "Invoked authenticate(%s) on worker %p, with available methods = [%s]", methods, this, available_methods);

	user_entry *u{};
	bool is_from_system{};
	shared_user shared_user;

	if (auto it = owner_.users_.find(name_); it != owner_.users_.end()) {
		u = &it->second;
	}
	else
	if (it = owner_.temp_users_.find(name_); it != owner_.temp_users_.end()) {
		u = &it->second;
	}
	else
	if (it = owner_.users_.find(owner_.users_.system_user_name); it != owner_.users_.end())  {
		if (it->second.enabled) {
			u = &it->second;
			is_from_system = true;
		}
	}

	if (!u)
		error = error::user_nonexisting;

	if (!error && !u->enabled)
		error = error::user_disabled;

	if (!error && !u->credentials.is_valid_for(u->methods, &logger_)) {
		logger_.log_u(logmsg::error, L"User \"%s\" has auth methods [%s] that do not match the credentials. Login is not possible. This is an internal error, inform the administrator.", name_, u->methods);
		error = error::internal;
	}

	if (!error) {
		// Check whether user's ip is disallowed
		if (u->disallowed_ips.contains(ip_, family_))
			error = error::ip_disallowed;

		if (!error) {
			for (const auto &n: u->groups) {
				if (const auto git = owner_.groups_.find(n); git != owner_.groups_.end()) {
					const auto &g = git->second;

					if (g.disallowed_ips.contains(ip_, family_)) {
						error = error::ip_disallowed;
						break;
					}
				}
			}
		}

		// If it is, check whether there are exceptions
		if (error) {
			if (u->allowed_ips.contains(ip_, family_))
				error = error::none;

			if (error) {
				for (const auto &n: u->groups) {
					if (const auto git = owner_.groups_.find(n); git != owner_.groups_.end()) {
						const auto &g = git->second;

						if (g.allowed_ips.contains(ip_, family_)) {
							error = error::none;
							break;
						}
					}
				}
			}
		}
	}

	if (!error && !available_methods.is_auth_possible()) {
		available_methods = u->methods;
	}

	if (!error && !methods.empty()) {
		if (logger_.should_log(logmsg::debug_verbose))
			logger_.log_u(logmsg::debug_verbose, "Authenticating user '%s'. Methods requested: %s. Available methods: [%s].", name_, methods, available_methods);

		if (!available_methods.can_verify(methods))
			error = error::auth_method_not_supported;

		if (error)
			logger_.log_u(logmsg::debug_verbose, "Authenticating user '%s' is not possible, no matching authentication methods are available.", name_);

		if (!error && available_methods.is_auth_necessary()) {
			impersonation_token impersonation_token;

			for (auto &method: methods) {
				if (!u->credentials.verify(name_, method, impersonation_token, logger_)) {
					error = error::invalid_credentials;

					if (logger_.should_log(logmsg::debug_verbose)) {
						logger_.log_u(logmsg::debug_verbose, "Auth method %s NOT passed for user '%s'. Invalid credentials.", method, name_);
					}

					break;
				}

				if (logger_.should_log(logmsg::debug_verbose)) {
					logger_.log_u(logmsg::debug_verbose, "Auth method %s passed for user '%s'.", method, name_);
				}

				if (auto m = method.is<method::password>()) {
					if (logger_.should_log(logmsg::debug_verbose)) {
						logger_.log_u(logmsg::debug_verbose, L"impersonation_token: { username: \"%s\", home: \"%s\" }", impersonation_token.username(), impersonation_token.home());
					}

					if (auto impersonation = u->credentials.password.get_impersonation(); impersonation && impersonation_token) {
						if (impersonation->login_only)
							impersonation_token = {};

						impersonation_token_ = std::move(impersonation_token);
					}
					else
					if (auto pwd = u->credentials.password.get(); pwd && !pwd->is<default_password>()) {
						logger_.log_u(logmsg::status, L"User '%s' has old style password, converting it into the new style one.", name_);
						*pwd = default_password(m->data);
						owner_.save_later();
					}
				}
			}

			// Only erase the methods from the list of available methods if all of the methods have been validated.
			if (!error && !methods.just_verify()) {
				for (auto &method: methods) {
					available_methods.set_verified(method);
				}
			}
		}
	}

	if (!error) {
		if ((methods.empty() && available_methods.is_auth_possible()) || available_methods.is_auth_necessary()) {
			if (logger_.should_log(logmsg::debug_debug))
				logger_.log_u(logmsg::debug_debug, "Authentication for user '%s' not complete. Remaning methods: [%s]", name_, available_methods);
		}
		else {
			if (!methods.empty()) {
				if (logger_.should_log(logmsg::debug_verbose))
					logger_.log_u(logmsg::debug_verbose, "Authentication for user '%s' is complete.", name_);

				// Maybe use the default impersonator
				if (!impersonation_token_) {
					if (auto imp = owner_.users_.default_impersonator.native(); imp && imp->enabled) {
						logger_.log_u(logmsg::debug_verbose, "User '%s' has no filesystem impersonator of its own but a default one for system user '%s' has been defined.", name_, imp->name);

						impersonation_token_ = imp->get_token();

						if (!impersonation_token_) {
							error = error::internal;

							logger_.log_u(logmsg::error, "Couldn't get the impersonation token for the default system user '%s', refusing to log in. Double check your settings!", imp->name);
						}
					}
				}

				if (!error) {
					if (impersonation_token_ && owner_.impersonator_exe_.empty()) {
						error = error::internal;

						logger_.log_u(logmsg::error, L"Filesystem impersonation has been requested, but no impersonator executable has been set, refusing to log in.");
					}
					else {
						shared_user = owner_.get_or_make_shared_user(name_, *u, is_from_system, std::move(impersonation_token_));
					}
				}
			}
		}
	}

	if (shared_user) {
		if (auto u = shared_user->lock()) {
			logger_.log_u(logmsg::debug_verbose, L"impersonation_token: { username: \"%s\", home: \"%s\" }", u->get_impersonation_token().username(), u->get_impersonation_token().home());

			auto op = std::make_unique<operation>(*this, std::move(shared_user), std::move(available_methods), error);

			// Clients of the authenticator MUST invoke stop_ongoing_authentications() when the handler for the authentication is about to be killed.
			// When that happens, the handler for the next async op will be deleted too, which in turn will avoid dispatching the completion event.
			// Hence, it's safe to use target.event_loop_, because the async handler will live less than the target handler, which will live less than its loop.
			return tvfs::async_autocreate_directories(u->mount_tree, u->impersonator, async_receive(owner_.async_handlers_.try_emplace(target_, target_->event_loop_).first->second)
			>> [t = target_, o = &owner_, op = std::move(op)]() mutable {
				t->send_event<operation::result_event>(*o, std::move(op));
			});
		}
		else {
			logger_.log_u(logmsg::error, L"Authentication succeeded but the shared_user couldn't be locked. This is an internal error, inform the administrator.");
			shared_user.reset();
			error = error::internal;
		}
	}

	auto op = std::make_unique<operation>(*this, std::move(shared_user), std::move(available_methods), error);
	target_->send_event<operation::result_event>(owner_, std::move(op));
}

}
