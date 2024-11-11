#ifndef GUI_CONFIG_PATHS_HPP
#define GUI_CONFIG_PATHS_HPP

#include "../filezilla/known_paths.hpp"

struct gui_config_paths: fz::known_paths::config {
	using config::config;

	FZ_KNOWN_PATHS_CONFIG_FILE(settings);
};

#endif // GUI_CONFIG_PATHS_HPP
