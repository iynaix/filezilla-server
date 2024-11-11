#ifndef FZ_CONFIGURATION_OLD_SERVER_CONFIG_HPP
#define FZ_CONFIGURATION_OLD_SERVER_CONFIG_HPP

#include <unordered_map>
#include <vector>

#include <libfilezilla/rate_limiter.hpp>

#include "../../filezilla/serialization/types/containers.hpp"
#include "../../filezilla/serialization/types/optional.hpp"

namespace fz::configuration::old {

struct server_config {
	// Generated with the following:
	//     cat configfile.xml | awk 'BEGIN { FS="\"" } /(Item|Option) [Nn]ame=/ { print "inline static const std::string", gensub(/ /, "_", "g", $2), "= \""  $2 "\";" } ' | sort -u

	struct key {
		inline static const std::string Active_ignore_local = "Active ignore local";
		inline static const std::string Admin_IP_Addresses = "Admin IP Addresses";
		inline static const std::string Admin_IP_Bindings = "Admin IP Bindings";
		inline static const std::string Admin_Password = "Admin Password";
		inline static const std::string Admin_port = "Admin port";
		inline static const std::string Allow_explicit_SSL = "Allow explicit SSL";
		inline static const std::string Allow_shared_write = "Allow shared write";
		inline static const std::string Autoban_attempts = "Autoban attempts";
		inline static const std::string Autoban_enable = "Autoban enable";
		inline static const std::string Autoban_time = "Autoban time";
		inline static const std::string Autoban_type = "Autoban type";
		inline static const std::string AutoCreate = "AutoCreate";
		inline static const std::string Buffer_Size = "Buffer Size";
		inline static const std::string Bypass_server_userlimit = "Bypass server userlimit";
		inline static const std::string Check_data_connection_IP = "Check data connection IP";
		inline static const std::string Comments = "Comments";
		inline static const std::string Custom_PASV_IP = "Custom PASV IP";
		inline static const std::string Custom_PASV_IP_server = "Custom PASV IP server";
		inline static const std::string Custom_PASV_IP_type = "Custom PASV IP type";
		inline static const std::string Custom_PASV_max_port = "Custom PASV max port";
		inline static const std::string Custom_PASV_min_port = "Custom PASV min port";
		inline static const std::string DirCreate = "DirCreate";
		inline static const std::string DirDelete = "DirDelete";
		inline static const std::string DirList = "DirList";
		inline static const std::string DirSubdirs = "DirSubdirs";
		inline static const std::string Disable_IPv6 = "Disable IPv6";
		inline static const std::string Download_Speedlimit = "Download Speedlimit";
		inline static const std::string Download_Speedlimit_Type = "Download Speedlimit Type";
		inline static const std::string Enabled = "Enabled";
		inline static const std::string Enable_HASH = "Enable HASH";
		inline static const std::string Enable_logging = "Enable logging";
		inline static const std::string Enable_SSL = "Enable SSL";
		inline static const std::string FileAppend = "FileAppend";
		inline static const std::string FileDelete = "FileDelete";
		inline static const std::string FileRead = "FileRead";
		inline static const std::string FileWrite = "FileWrite";
		inline static const std::string Force_explicit_SSL = "Force explicit SSL";
		inline static const std::string Force_PROT_P = "Force PROT P";
		inline static const std::string ForceSsl = "ForceSsl";
		inline static const std::string Force_TLS_session_resumption = "Force TLS session resumption";
		inline static const std::string Group = "Group";
		inline static const std::string Hide_Welcome_Message = "Hide Welcome Message";
		inline static const std::string Implicit_SSL_ports = "Implicit SSL ports";
		inline static const std::string Initial_Welcome_Message = "Initial Welcome Message";
		inline static const std::string IP_Bindings = "IP Bindings";
		inline static const std::string IP_Filter_Allowed = "IP Filter Allowed";
		inline static const std::string IP_Filter_Disallowed = "IP Filter Disallowed";
		inline static const std::string IP_Limit = "IP Limit";
		inline static const std::string IsHome = "IsHome";
		inline static const std::string Logfile_delete_time = "Logfile delete time";
		inline static const std::string Logfile_type = "Logfile type";
		inline static const std::string Login_Timeout = "Login Timeout";
		inline static const std::string Logsize_limit = "Logsize limit";
		inline static const std::string Maximum_user_count = "Maximum user count";
		inline static const std::string Minimum_TLS_version = "Minimum TLS version";
		inline static const std::string Mode_Z_allow_local = "Mode Z allow local";
		inline static const std::string Mode_Z_disallowed_IPs = "Mode Z disallowed IPs";
		inline static const std::string Mode_Z_max_level = "Mode Z max level";
		inline static const std::string Mode_Z_min_level = "Mode Z min level";
		inline static const std::string Mode_Z_Use = "Mode Z Use";
		inline static const std::string Network_Buffer_Size = "Network Buffer Size";
		inline static const std::string No_External_IP_On_Local = "No External IP On Local";
		inline static const std::string No_Transfer_Timeout = "No Transfer Timeout";
		inline static const std::string Number_of_Threads = "Number of Threads";
		inline static const std::string Pass = "Pass";
		inline static const std::string Salt = "Salt";
		inline static const std::string Serverports = "Serverports";
		inline static const std::string Service_display_name = "Service display name";
		inline static const std::string Service_name = "Service name";
		inline static const std::string Show_Pass_in_Log = "Show Pass in Log";
		inline static const std::string SSL_Certificate_file = "SSL Certificate file";
		inline static const std::string SSL_Key_file = "SSL Key file";
		inline static const std::string SSL_Key_Password = "SSL Key Password";
		inline static const std::string Timeout = "Timeout";
		inline static const std::string Upload_Speedlimit_Type = "Upload Speedlimit Type";
		inline static const std::string Upload_Speedlimit = "Upload Speedlimit";
		inline static const std::string Use_custom_PASV_ports = "Use custom PASV ports";
		inline static const std::string User_Limit = "User Limit";
	};

	struct key_name {
		static constexpr char Name[] = "Name";
		static constexpr char Dir[] = "Dir";
		static constexpr char name[] = "name";
	};

	using IP = std::string;

	struct UserOrGroup {
		using Options = fz::serialization::with_key_name<std::unordered_map<std::string /*Name*/, std::string /*value*/>, key_name::Name>;

		struct Permission {
			std::vector<std::string> aliases;
			Options options;
		};

		using Permissions = fz::serialization::with_key_name<std::unordered_map<fz::native_string /*Dir*/, Permission>, key_name::Dir>;

		struct IpFilter {
			std::vector<IP> allowed{};
			std::vector<IP> disallowed{};
		};

		struct SpeedLimits {
			enum type {
				default_limits  = 0,
				no_limits       = 1,
				constant_limits = 2,
				rules_limits    = 3
			};

			type dl_type;
			type ul_type;
			fz::rate::type dl_limit;
			fz::rate::type ul_limit;
		};

		IpFilter ip_filter{};
		SpeedLimits speed_limits{};
		Options options{};
		Permissions permissions{};
	};

	using Settings = fz::serialization::with_key_name<std::unordered_map<std::string /*name*/, std::string>, key_name::name>;
	using Users = fz::serialization::with_key_name<std::unordered_map<std::string /*Name*/, UserOrGroup>, key_name::Name>;
	using Groups = fz::serialization::with_key_name<std::unordered_map<std::string /*Name*/, UserOrGroup>, key_name::Name>;

	enum class ServerSpeedLimits {
		no_limits = 0,
		constant_limits = 1,
		rules_limits = 2
	};

	Settings settings{};
	Users users{};
	Groups groups{};
};

template <typename Archive>
void serialize(Archive &ar, server_config::UserOrGroup::IpFilter &v) {
	using namespace fz::serialization;

	ar(nvp(v.allowed, "Allowed", "IP"), nvp(v.disallowed, "Disallowed", "IP"));
}

template <typename Archive>
void serialize(Archive &ar, server_config::UserOrGroup::SpeedLimits &v) {
	using namespace fz::serialization;

	ar.attribute(v.dl_type, "DlType")
	  .attribute(v.dl_limit, "DlLimit")
	  .attribute(v.ul_type, "UlType")
	  .attribute(v.ul_limit, "UlLimit");
}


template <typename Archive>
void serialize(Archive &ar, server_config::UserOrGroup::Permission &v) {
	using namespace fz::serialization;

	ar(
		optional_nvp(v.aliases, "Aliases", "Alias"),
		nvp(v.options, "", "Option")
	);
}

template <typename Archive>
void serialize(Archive &ar, server_config::UserOrGroup &v) {
	using namespace fz::serialization;

	ar(
		nvp(v.ip_filter, "IpFilter"),
		nvp(v.speed_limits, "SpeedLimits"),
		nvp(v.options, "", "Option"),
		nvp(v.permissions, "Permissions", "Permission")
	);
}


template <typename Archive>
void serialize(Archive &ar, server_config &v) {
	using namespace fz::serialization;

	ar(
		nvp(v.settings, "Settings", "Item"),
		optional_nvp(v.users, "Users", "User"),
		optional_nvp(v.groups, "Groups", "Group")
	);
}

}

#endif // FZ_CONFIGURATION_OLD_CONFIG_HPP
