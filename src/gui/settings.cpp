#include <libfilezilla/glue/wx.hpp>

#include <wx/weakref.h>
#include <wx/display.h>

#include "../filezilla/util/parser.hpp"

#include "settings.hpp"
#include "helpers.hpp"

struct Settings::config_paths: fz::known_paths::config
{
	using config::config;

	FZ_KNOWN_PATHS_CONFIG_FILE(settings);
};

void Settings::Save()
{
	using namespace fz::serialization;

	auto settings_xml = get_config_paths().settings(fz::file::writing);
	xml_output_archive::file_saver saver(fz::to_native(settings_xml));

	if (xml_output_archive{saver}(nvp{get_options(), ""}))
		;//wxLogMessage(_S("Settings written to %s."), fz::to_wxString(settings_xml));
	else
		wxMsg::Error(_S("Failed writing settings to %s."), fz::to_wxString(settings_xml.str()));
}

void Settings::Reset()
{
	get_options().reset_optionals();
	Save();
}


Settings::options *Settings::operator->()
{
	return &get_options();
}

Settings::options &Settings::get_options()
{
	static options opts;
	return opts;
}

Settings::config_paths &Settings::get_config_paths()
{
	static config_paths config_paths{fzT("filezilla-server-gui")};
	return config_paths;
}

bool Settings::Load(int argc, char *argv[])
{
	using namespace fz::serialization;
	bool write_config = false;

	argv_input_archive ar{argc, argv, {
		{get_config_paths().settings(fz::file::reading), true}
	}};

	ar(
		nvp{get_options(), ""},
		value_info{optional_nvp{write_config, "write-config"}, "Write the passed in configuration to the proper files"}
	).check_for_unhandled_options();

	if (!ar) {
		wxMsg::Error(_S("%s."), ar.error().description());
		return false;
	}

	if (write_config) {
		Save();
		return false;
	}

	return true;
}

static unsigned int find_best_display_id(unsigned int id)
{
	if (id > 0 && id <= wxDisplay::GetCount())
			return id-1;

	for (unsigned int cur = 0, end = wxDisplay::GetCount(); cur != end; ++cur) {
		if (wxDisplay(cur).IsPrimary())
			return cur;
	}

	return 0;
}

static bool is_visible_in_any_display(const wxRect &rect)
{
	for (unsigned int cur = 0, end = wxDisplay::GetCount(); cur != end; ++cur) {
		auto disp_rect = wxDisplay(cur).GetGeometry();
		int x = rect.GetX();
		int y = rect.GetY();

		if (x >= disp_rect.GetX() && x <= disp_rect.GetRight() && y >= disp_rect.GetY() && y <= disp_rect.GetBottom())
			return true;
	}

	return false;
}

wxRect Settings::options::geometry::spec2rect(std::string_view spec) {
	fz::util::parseable_range r(spec);

	// First try to parse the display id
	unsigned int display_id = 0;

	auto old_it = r.it;
	if (!(parse_int(r, display_id) && lit(r, ':'))) {
		display_id = {};
		r.it = old_it;
	}

	// Then try to parse the sizes
	std::optional<int> width, height;

	old_it = r.it;
	if (!(parse_int(r, width) && lit(r, std::string_view("xX")) && parse_int(r, height)) || width < 0 || height < 0) {
		width = height = {};
		r.it = old_it;
	}

	// Then try to parse the offsets
	char xsign = 0, ysign = 0;
	int xoff = 0, yoff = 0;

	old_it = r.it;
	if (!(parse_lit(r, xsign, "+-") && parse_int(r, xoff) && parse_lit(r, ysign, "+-")  && parse_int(r, yoff))) {
		xsign = ysign = 0;
		r.it = old_it;
	}

	wxRect window_rect;

	for (;;) {
		wxRect display_rect = wxDisplay(find_best_display_id(display_id)).GetGeometry();

		if (!width) {
			width = (unsigned int)display_rect.GetWidth() / 2;
			height = (unsigned int)display_rect.GetHeight() / 2;
		}

		if (!xsign) {
			xsign = ysign = '+';
			xoff = *width < display_rect.GetWidth() ? (display_rect.GetWidth()-*width)/2 : 0;
			yoff = *height < display_rect.GetHeight() ? (display_rect.GetHeight()-*height)/2 : 0;
		}

		if (xsign == '+')
			window_rect.SetX(display_rect.GetX() + xoff);
		else
			window_rect.SetX(display_rect.GetX()+display_rect.GetWidth() - (xoff + *width));

		if (ysign == '+')
			window_rect.SetY(display_rect.GetY() + yoff);
		else
			window_rect.SetY(display_rect.GetY()+display_rect.GetHeight() - (yoff + *height));

		if (is_visible_in_any_display(window_rect)) {
			window_rect.SetSize({*width, *height});
			break;
		}

		xsign = ysign = 0;
	}

	return window_rect;
}

std::string Settings::options::geometry::rect2spec(const wxRect &rect)
{
	std::string spec;

	if (rect.GetWidth() >= 0 && rect.GetHeight() >= 0)
		spec = fz::sprintf("%dx%d", rect.GetWidth(), rect.GetHeight());

	spec += fz::sprintf("+%d+%d", rect.GetX(), rect.GetY());

	return spec;
}


#define PERSIST_DEBUG 0

#if PERSIST_DEBUG
static void dump_size(const char *msg, const wxSize &size)
{
	std::cerr << "SIZE [" << msg << "]: ("<< size.GetWidth() << ", " << size.GetHeight() << ")" << std::endl;
}

static void dump_position(const char *msg, const wxPoint &p)
{
	std::cerr << "POSITION [" << msg << "]: ("<< p.x << ", " << p.y << ")" << std::endl;
}
#endif


void PersistWindow(wxTopLevelWindow *win)
{
	wxASSERT(win != nullptr && !win->GetName().empty());

	struct Persistor: wxWindow
	{
		wxTopLevelWindow *parent()
		{
			return static_cast<wxTopLevelWindow*>(GetParent());
		}

		Persistor(wxTopLevelWindow *win)
			: wxWindow(win, wxID_ANY)
			, geometry_(Settings()->geometries()[(fz::to_wstring(win->GetName()))])
		{
			Hide();

			if (!restore())
				parent()->Bind(wxEVT_SHOW, &Persistor::OnShow, this);
		}

		bool restore()
		{
			if (!parent()->IsShown())
				return false;

			parent()->Move(geometry_.rect.GetPosition(), wxSIZE_ALLOW_MINUS_ONE);
			parent()->SetClientSize(geometry_.rect.GetSize());

			if (geometry_.maximized) {
#ifdef __WXOSX__
				parent()->ShowFullScreen(true, wxFULLSCREEN_NOMENUBAR);
#else
				parent()->Maximize();
#endif
			}

			parent()->Bind(wxEVT_SIZE, &Persistor::OnSize, this);
			parent()->Bind(wxEVT_MOVE, &Persistor::OnMove, this);

			return true;
		}

		void OnSize(wxSizeEvent &ev)
		{
			ev.Skip();

#ifdef __WXOSX__
			geometry_.maximized = parent()->IsFullScreen();
#else
			geometry_.maximized = parent()->IsMaximized();
#endif

			if (!geometry_.maximized && !parent()->IsIconized() && parent()->IsShown())
				geometry_.rect.SetSize(parent()->GetClientSize());
		}

		void OnMove(wxMoveEvent &ev)
		{
			ev.Skip();

#ifdef __WXOSX__
			geometry_.maximized = parent()->IsFullScreen();
#else
			geometry_.maximized = parent()->IsMaximized();
#endif

			if (!geometry_.maximized && !parent()->IsIconized() && parent()->IsShown())
				geometry_.rect.SetPosition(parent()->GetPosition());
		}

		void OnShow(wxShowEvent &ev)
		{
			ev.Skip();

			if (restore())
				parent()->Unbind(wxEVT_SHOW, &Persistor::OnShow, this);
		}

		~Persistor() override
		{
			Settings().Save();
		}

	private:
		Settings::options::geometry &geometry_;
	};

	new Persistor(win);
}
