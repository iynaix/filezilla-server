#ifndef SERVER_CONFIG_PATHS_H
#define SERVER_CONFIG_PATHS_H

#include "../filezilla/known_paths.hpp"

struct server_config_paths: fz::known_paths::config {
	using config::config;

	FZ_KNOWN_PATHS_CONFIG_FILE(settings);
	FZ_KNOWN_PATHS_CONFIG_FILE(users);
	FZ_KNOWN_PATHS_CONFIG_FILE(groups);
	FZ_KNOWN_PATHS_CONFIG_FILE(disallowed_ips);
	FZ_KNOWN_PATHS_CONFIG_FILE(allowed_ips);

	FZ_KNOWN_PATHS_CONFIG_DIR(certificates);
	FZ_KNOWN_PATHS_CONFIG_DIR(update);
	FZ_KNOWN_PATHS_CONFIG_DIR(webui);
};

#endif // SERVER_CONFIG_PATHS_H
