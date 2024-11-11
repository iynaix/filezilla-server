#ifndef WRAPPEDTEXT_HPP
#define WRAPPEDTEXT_HPP

#include <wx/panel.h>
#include <wx/stattext.h>

class WrappedText: public wxPanel
{
public:
	WrappedText(wxWindow *parent, const wxString &text, int style = 0);

	bool SetForegroundColour(const wxColour &colour) override;
	bool SetBackgroundColour(const wxColour &colour) override;
	bool SetFont(const wxFont &font) override;

protected:
	void SetLabel(const wxString &label) override;

	bool InformFirstDirection(int dir, int size, int available_other_dir) override;

	wxSize DoGetBestClientSize() const override;
	bool AcceptsFocus() const override;
	bool AcceptsFocusFromKeyboard() const override;

private:
	wxStaticText *text_{};
	bool informed_{};
	wxSize extent_;
};

#endif // WRAPPEDTEXT_HPP
