#include "converter.hpp"
#include "server_config.hpp"

namespace fz::configuration::old {

namespace {

	template <typename T>
	std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, T>
	as(const std::string &s)
	{
		return fz::to_integral<T>(s);
	}

	template <typename T>
	std::enable_if_t<std::is_same_v<std::string, T>, const std::string &>
	as(const std::string &s)
	{
		return s;
	}

	template <typename T>
	[[maybe_unused]] std::enable_if_t<std::is_same_v<std::wstring, T>, T>
	as(const std::string &s)
	{
		return fz::to_wstring(s);
	}

	template <typename T>
	[[maybe_unused]] std::enable_if_t<std::is_same_v<std::string_view, T>, T>
	as(const std::string &s)
	{
		return s;
	}

	template <typename T>
	[[maybe_unused]] std::enable_if_t<std::is_same_v<fz::duration, T>, T>
	as(const std::string &s)
	{
		return fz::duration::from_seconds(as<std::int64_t>(s));
	}

	struct any: std::string
	{
		using std::string::string;

		any(const std::string &s)
			: std::string(s)
		{}

		template <typename T>
		operator T() const { return as<T>(*this); }
	};

	template <typename T>
	std::enable_if_t<std::is_same_v<any, T>, any>
	as(const std::string &s)
	{
		return s;
	}

	template <typename T = any, typename C, typename K>
	T get(C && c, K && k, T && d = {})
	{
		auto it = std::forward<C>(c).find(std::forward<K>(k));
		if (it != std::forward<C>(c).end())
			return as<T>(it->second);

		return std::forward<T>(d);
	}

	void convert(const server_config::UserOrGroup::Permissions &old, fz::tvfs::mount_table &mt, logger_interface &logger)
	{
		auto get_access = [](const server_config::UserOrGroup::Permission &p) {
			bool can_modify =
				get<bool>(p.options, server_config::key::FileWrite)  ||
				get<bool>(p.options, server_config::key::FileDelete) ||
				get<bool>(p.options, server_config::key::FileAppend) ||
				get<bool>(p.options, server_config::key::DirCreate)  ||
				get<bool>(p.options, server_config::key::DirDelete);

			bool can_read =
					get<bool>(p.options, server_config::key::FileRead)  ||
					get<bool>(p.options, server_config::key::DirList);

			return can_modify
				? fz::tvfs::mount_point::read_write
				: can_read
					? fz::tvfs::mount_point::read_only
					: fz::tvfs::mount_point::disabled;
		};

		auto get_recursive = [](const server_config::UserOrGroup::Permission &p) {
			bool can_modify_structure =
				get<bool>(p.options, server_config::key::DirCreate) ||
				get<bool>(p.options, server_config::key::DirDelete);

			bool is_recursive =
				get<bool>(p.options, server_config::key::DirSubdirs);

			return is_recursive
				? can_modify_structure
					? fz::tvfs::mount_point::apply_permissions_recursively_and_allow_structure_modification
					: fz::tvfs::mount_point::apply_permissions_recursively
				: fz::tvfs::mount_point::do_not_apply_permissions_recursively;
		};

		auto get_flags = [](const server_config::UserOrGroup::Permission &p) {
			fz::tvfs::mount_point::flags_t flags{};

			if (get<bool>(p.options, server_config::key::AutoCreate))
				flags |= fz::tvfs::mount_point::autocreate;

			return flags;
		};

		if (old.begin() == old.end()) {
			return;
		}

		auto home_it = std::find_if(old.begin(), old.end(), [](const auto &i) {
			return get<bool>(i.second.options, server_config::key::IsHome);
		});

		if (home_it == old.end()) {
			logger.log_u(logmsg::warning, L"Home directory not found in permissions. Ignoring all of them.");
			return;
		}

		for (auto it = old.begin(); it != old.end(); ++it) {
			auto &native_path = it->first;
			auto &p = it->second;

			auto aliases = [&]() -> const std::vector<std::string> {
				if (p.aliases.empty()) {
					if (it == home_it) {
						return { "/" };
					}

					if (fz::starts_with(native_path, home_it->first)) {
						return { fz::replaced_substrings(fz::to_utf8(native_path.substr(home_it->first.size())), '\\', '/') };
					}

					return {};
				}

				return p.aliases;
			}();

			fz::util::fs::windows_native_path wnp(fz::tvfs::placeholders::convert_old_style_to_new(native_path));

			if (!wnp.is_absolute()) {
				if (native_path.back() == ':')
					wnp = native_path + fzT("\\");

				if (!wnp.is_absolute()) {
					logger.log_u(logmsg::warning, L"Permission path [%s] is not absolute. Ignoring it.", native_path);
					continue;
				}
			}

			for (const auto &tvfs_path: aliases) {
				if (!fz::util::fs::unix_path(tvfs_path).is_absolute()) {
					logger.log_u(logmsg::warning, L"Alias path [%s] is not absolute. Ignoring it.", tvfs_path);
					continue;
				}

				mt.push_back({tvfs_path, wnp, get_access(p), get_recursive(p), get_flags(p)});
			}
		}
	}

	template <server_config::UserOrGroup::SpeedLimits::type SpeedLimitsType>
	bool match_speed_limit(server_config::UserOrGroup::SpeedLimits::type v, const std::string_view &what, logger_interface &logger) {
		switch (v) {
			case server_config::UserOrGroup::SpeedLimits::rules_limits:
				logger.log_u(logmsg::warning, L"parsing speed limits for [%s]: rule based speed limits aren't currently supported. Ignoring.", what);
				break;

			case server_config::UserOrGroup::SpeedLimits::constant_limits:
				logger.log_u(logmsg::warning, L"parsing speed limits for [%s]: constant speed limits don't override the Server or the parent Group ones, they work together with them. This is different than how the old server worked.", what);
				break;

			case server_config::UserOrGroup::SpeedLimits::no_limits:
				logger.log_u(logmsg::warning, L"parsing speed limits for [%s]: even if this entry has no limits, the parent Group or Server limits still apply. This is different than how the old server worked.", what);
				break;

			case server_config::UserOrGroup::SpeedLimits::default_limits:
				break;
		}

		return v == SpeedLimitsType;
	}

	template <server_config::ServerSpeedLimits SpeedLimitsType>
	bool match_speed_limit(server_config::ServerSpeedLimits v, const std::string_view &what, logger_interface &logger) {
		switch (v) {
			case server_config::ServerSpeedLimits::rules_limits:
				logger.log_u(logmsg::warning, L"parsing speed limits for [%s]: rule based speed limits aren't currently supported. Ignoring.", what);
				break;

			case server_config::ServerSpeedLimits::constant_limits:
			case server_config::ServerSpeedLimits::no_limits:
				break;
		}

		return v == SpeedLimitsType;
	}

	void convert(const server_config::UserOrGroup::SpeedLimits &old, fz::authentication::file_based_authenticator::rate_limits &rl, const std::string_view &what, logger_interface &logger)
	{
		if (match_speed_limit<server_config::UserOrGroup::SpeedLimits::constant_limits>(old.dl_type, fz::sprintf("%s/download", what), logger))
			rl.session_inbound = old.dl_limit * 1024;

		if (match_speed_limit<server_config::UserOrGroup::SpeedLimits::constant_limits>(old.ul_type, fz::sprintf("%s/upload", what), logger))
			rl.session_outbound = old.ul_limit * 1024;
	}

	auto on_ip_convert_error(std::string what, logger_interface &logger) {
		return [what = std::move(what), &logger] (std::size_t, const std::string_view &ip) {
			logger.log_u(logmsg::warning, L"Ignoring bad IP/range [%s] while converting [%s].", ip, what);
			return true;
		};
	}

	void convert(const server_config::UserOrGroup &old, fz::authentication::any_password &any_password)
	{
		const std::string &hash = get(old.options, server_config::key::Pass);
		const std::string &salt = get(old.options, server_config::key::Salt);

		if (!hash.empty()) {
			if (salt.empty())
				any_password = fz::authentication::password::md5::from_hash(fz::hex_decode<std::string>(hash));
			else
				any_password = fz::authentication::password::sha512::from_hash_and_salt(fz::hex_decode<std::string>(hash), salt);
		}
	}
}

bool converter::extract(fz::authentication::file_based_authenticator::groups &groups, fz::authentication::file_based_authenticator::groups::value_type **speed_limited_group)
{
	for (const auto &[name, o]: config_.groups) {
		auto &n = groups[name];

		n.description = get<std::string>(o.options, server_config::key::Comments);

		convert(o.permissions, n.mount_table, logger_);
		convert(o.speed_limits, n.rate_limits, fz::sprintf("Group %s", name), logger_);

		if (!convert(o.ip_filter.disallowed, n.disallowed_ips, on_ip_convert_error(fz::sprintf("Group %s, IpFilter.Disallowed", name), logger_)))
			return false;

		if (!convert(o.ip_filter.allowed, n.allowed_ips, on_ip_convert_error(fz::sprintf("Group %s, IpFilter.Allowed", name), logger_)))
			return false;
	}

	if (speed_limited_group) {
		server_config::ServerSpeedLimits server_dl_type = get(config_.settings, server_config::key::Download_Speedlimit_Type);
		server_config::ServerSpeedLimits server_ul_type = get(config_.settings, server_config::key::Upload_Speedlimit_Type);

		bool server_has_download_limits =  match_speed_limit<server_config::ServerSpeedLimits::constant_limits>(server_dl_type, fz::sprintf("Settings/%s", server_config::key::Download_Speedlimit_Type), logger_);
		bool server_has_upload_limits = match_speed_limit<server_config::ServerSpeedLimits::constant_limits>(server_ul_type, fz::sprintf("Settings/%s", server_config::key::Upload_Speedlimit_Type), logger_);

		if (server_has_download_limits || server_has_upload_limits) {
			std::string name = ":SpeedLimitedGroup:";

			while (config_.groups.count(name) != 0)
				name.append(1, ':');

			auto &&g = groups.insert(fz::authentication::file_based_authenticator::groups::value_type(name, {}));
			*speed_limited_group = &*g.first;

			if (server_has_download_limits) {
				g.first->second.rate_limits.session_inbound = get(config_.settings, server_config::key::Download_Speedlimit);
				g.first->second.rate_limits.session_inbound *= 1024;
			}

			if (server_has_upload_limits) {
				g.first->second.rate_limits.session_outbound = get(config_.settings, server_config::key::Upload_Speedlimit);
				g.first->second.rate_limits.session_outbound *= 1024;
			}
		}
	}

	return true;
}

bool converter::extract(fz::authentication::file_based_authenticator::users &users, fz::authentication::file_based_authenticator::groups::value_type *speed_limited_group)
{
	for (const auto &[name, o]: config_.users) {
		auto &n = users[name];

		n.description = get(o.options, server_config::key::Comments);
		n.enabled = get(o.options, server_config::key::Enabled, true);

		convert(o.permissions, n.mount_table, logger_);
		convert(o.speed_limits, n.rate_limits, fz::sprintf("User %s", name), logger_);
		fz::authentication::any_password pw;
		convert(o, pw);
		n.credentials.password = std::move(pw);

		if (!convert(o.ip_filter.disallowed, n.disallowed_ips, on_ip_convert_error(fz::sprintf("User %s, IpFilter.Disallowed", name), logger_)))
			return false;

		if (!convert(o.ip_filter.allowed, n.allowed_ips, on_ip_convert_error(fz::sprintf("User %s, IpFilter.Allowed", name), logger_)))
			return false;

		std::string group = get(o.options, server_config::key::Group);
		if (!group.empty())
			n.groups.push_back(group);

		if (speed_limited_group)
			n.groups.push_back(speed_limited_group->first);
	}

	return true;
}

bool converter::extract(server_settings &s)
{
	auto &o = config_.settings;

	static const std::vector<std::string> any_ip = { "*" };

	{   /*** Control addresses and ports ***/
		auto control_ips = fz::strtok(get(o, server_config::key::IP_Bindings), ' ');
		auto control_ports = fz::strtok(get(o, server_config::key::Serverports), ' ');
		auto implicit_tls_control_ports = fz::strtok(get(o, server_config::key::Implicit_SSL_ports), ' ');

		auto push_ip = [&control_ports, &implicit_tls_control_ports, &s](const std::string &ip) {
			for (const auto &p: control_ports)
				s.ftp_server.listeners_info().push_back({{ip, as<unsigned>(p)}, fz::ftp::session::allow_tls});

			for (const auto &p: implicit_tls_control_ports)
				s.ftp_server.listeners_info().push_back({{ip, as<unsigned>(p)}, fz::ftp::session::implicit_tls});
		};

		for (const auto &i: control_ips.empty() ? any_ip : control_ips) {
			if (i == "*") {
				push_ip("0.0.0.0");
				push_ip("::");
			}
			else
				push_ip(i);
		}
	}

	{   /*** Admin addresses and ports ***/
		auto admin_ips = fz::strtok(get(o, server_config::key::Admin_IP_Bindings), ' ');
		auto admin_port = get<unsigned>(o, server_config::key::Admin_port);

		s.admin.local_port = admin_port;

		for (const auto &i: admin_ips)
			s.admin.additional_address_info_list.push_back({{i, admin_port}, fz::ftp::session::implicit_tls});
	}

	/*** TLS info ***/
	s.ftp_server.sessions().tls.cert = fz::securable_socket::omni_cert_info {
		tls_filepath(get(o, server_config::key::SSL_Certificate_file)),
		tls_filepath(get(o, server_config::key::SSL_Key_file)),
		get(o, server_config::key::SSL_Key_Password),
		fz::securable_socket::omni_cert_info::sources::provided{}
	};

	s.ftp_server.sessions().tls.min_tls_ver = get(o, server_config::key::Minimum_TLS_version);
	s.admin.tls.min_tls_ver = get(o, server_config::key::Minimum_TLS_version);

	/*** Misc ***/
	s.protocols.performance.number_of_session_threads = get(o, server_config::key::Number_of_Threads);
	s.protocols.performance.receive_buffer_size = get(o, server_config::key::Network_Buffer_Size);
	s.protocols.performance.send_buffer_size = get(o, server_config::key::Network_Buffer_Size);
	s.protocols.timeouts.login_timeout = get(o, server_config::key::Login_Timeout);
	s.protocols.timeouts.activity_timeout = get(o, server_config::key::No_Transfer_Timeout);

	return true;
}

bool converter::extract(fz::tcp::binary_address_list &disallowed_ips, fz::tcp::binary_address_list &allowed_ips)
{
	return
		convert(get<std::string>(config_.settings, server_config::key::IP_Filter_Disallowed), disallowed_ips, on_ip_convert_error(fz::sprintf("Settings/%s", server_config::key::IP_Filter_Disallowed), logger_)) &&
		convert(get<std::string>(config_.settings, server_config::key::IP_Filter_Allowed), allowed_ips, on_ip_convert_error(fz::sprintf("Settings/%s", server_config::key::IP_Filter_Allowed), logger_));
}

}
