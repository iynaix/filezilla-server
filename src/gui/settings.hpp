#ifndef SETTINGS_H
#define SETTINGS_H

#include <unordered_map>

#include <wx/gdicmn.h>

#include "../filezilla/serialization/archives/xml.hpp"
#include "../filezilla/serialization/archives/argv.hpp"
#include "../filezilla/serialization/types/logger_file_options.hpp"
#include "../filezilla/util/serializable.hpp"
#include "../filezilla/known_paths.hpp"
#include "../filezilla/string.hpp"

#include "connectiondialog.hpp"

class Settings
{
	static constexpr char key_name[] = "name";

public:
	struct options: public fz::util::serializable<options, fz::serialization::xml_output_archive, fz::serialization::xml_input_archive, fz::serialization::argv_input_archive>
	{
		mandatory <ConnectionDialog::servers_info> servers_info =
			name("").
			info("Information about the FileZilla FTP servers to connect to.");

		optional <fz::logger::file::options> logger =
			name("logger").
			info("Logging options");

		optional  <std::string> locale =
			value("").
			name("locale").
			info("The GUI's locale. By default, the one defined by the appropriate environment variables is used.");

		optional <bool> start_minimized =
			value(false).
			name("start_minimized").
			info("True if the UI window has to be started in minimized mode, false otherwise.");

		enum tray_icon_mode_t
		{
			tray_icon_disabled,
			tray_icon_enabled,
			minimize_to_tray
		};

		optional <tray_icon_mode_t> tray_icon_mode =
	#ifdef FZ_UNIX
			value(tray_icon_disabled).
	#else
			value(tray_icon_enabled).
	#endif
			name("tray_icon_mode").
			info("Tray icon mode: one of 0 (disabled), 1 (enabled), 2 (enabled, minimize to tray).");

		optional <bool> native_path_warn =
			value(true).
			name("native_path_warn").
			info("True if the mount table editor has to warn about being cautious when setting up native paths.");

		optional <fz::util::fs::native_path> working_dir =
			name("working_dir").
			info("Working directory");

		optional <fz::util::fs::native_path> updates_dir =
			value(fz::known_paths::user::downloads()).
			name("updates_dir").
			info("Directory where updates will be downloaded");

		optional <bool> automatically_download_updates =
			name("automatically_download_updates").
			info("True if the updates must be automatically downloaded where <updates_dir> states. If <updates_dir> is empty, no download will happen.");

		struct geometry
		{
			bool maximized{};
			wxRect rect = spec2rect("");

			template <typename Archive>
			void serialize(Archive &ar)
			{
				using namespace fz::serialization;

				std::string spec;

				if constexpr (trait::is_output_v<Archive>)
					spec = rect2spec(rect);

				ar.optional_attribute(maximized, "maximized")(
					nvp(spec, "")
				);

				if constexpr (trait::is_input_v<Archive>)
					rect = spec2rect(spec);
			}

		private:
			static wxRect spec2rect(std::string_view spec);
			static std::string rect2spec(const wxRect &spec);
		};

		using geometry_map = fz::serialization::with_key_name<std::unordered_map<std::wstring /*window name*/, geometry>, key_name>;

		optional <geometry_map> geometries =
			names("", "geometry").
			info("Position and sizes of the app windows. The geometry spec is as follows: [display_id:][width{xX}height][{+-}xoff{+-}yoff]. Display_id and xy offsets only apply to top windows.");

		struct columns_properties
		{
			std::vector<int> min_widths;
			std::vector<int> widths;

			template <typename Archive>
			void serialize(Archive &ar)
			{
				using namespace fz::serialization;

				std::string s_min_widths;
				std::string s_widths;

				if constexpr (trait::is_output_v<Archive>) {
					s_min_widths = fz::join<std::string>(min_widths, ",");
					s_widths = fz::join<std::string>(widths, ",");
				}

				ar(
					value_info(nvp(s_min_widths, "min_widths"), "Minimum widths of the columns, separated by spaces or commas."),
					value_info(nvp(s_widths, "widths"), "Actual widths of the columns, separated by spaces or commas.")
				);

				if constexpr (trait::is_input_v<Archive>) {
					if (ar) {
						for (auto t: fz::strtokenizer(s_min_widths, ", ", true))
							min_widths.push_back(std::max(0, fz::to_integral<int>(t, 80)));

						for (auto t: fz::strtokenizer(s_widths, ", ", true))
							widths.push_back(std::max(0, fz::to_integral<int>(t, 80)));

						if (min_widths.size() <= widths.size())
							widths.resize(min_widths.size());
						else
							min_widths.resize(widths.size());
					}
				}
			}
		};

		using columns_map = fz::serialization::with_key_name<std::unordered_map<std::wstring /*window name*/, columns_properties>, key_name>;

		optional <columns_map> columns =
			names("", "columns").
			info("Properties of the columns of the named grid or listctl.");

		using sashes_map = fz::serialization::with_key_name<std::unordered_map<std::wstring /*window name*/, int>, key_name>;

		optional <sashes_map> sashes =
			names("", "sash").
			info("Position of the sash in the named splitted window.");
	};

public:
	bool Load(int argc, char *argv[]);
	void Save();
	void Reset();

	options *operator->();

private:
	struct config_paths;

	options &get_options();
	config_paths &get_config_paths();
};

void PersistWindow(wxTopLevelWindow *win);

#endif // SETTINGS_H
