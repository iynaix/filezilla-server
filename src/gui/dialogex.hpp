#ifndef DIALOGEX_HPP
#define DIALOGEX_HPP

#include <libfilezilla/glue/wx.hpp>

#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/richmsgdlg.h>

#include "../filezilla/util/traits.hpp"

template <typename BaseDialog = wxDialog, typename SFINAE = void>
class wxDialogEx;

struct wxDialogExTag
{
private:
	wxDialogExTag() = default;

	template <typename BaseDialog, typename SFINAE>
	friend class wxDialogEx;
};

template <typename BaseDialog>
class wxDialogEx<BaseDialog, std::enable_if_t<std::is_base_of_v<wxDialog, BaseDialog> && !std::is_base_of_v<wxDialogExTag, BaseDialog>>>
	: public BaseDialog, wxDialogExTag
{
public:
	using BaseDialog::BaseDialog;

	int ShowModal() override
	{
		if constexpr (!std::is_base_of_v<wxMessageDialog, BaseDialog> && !std::is_base_of_v<wxRichMessageDialog, BaseDialog>)
			this->CenterOnParent();

		// See https://wx-dev.narkive.com/H4M3aAH7/wxmsw-freeze-with-modal-dialogs-and-popup-menus#post1
		#ifdef __WXMSW__
			// All open menus need to be closed or app will become unresponsive.
			::EndMenu();

			// For same reason release mouse capture.
			// Could happen during drag&drop with notification dialogs.
			::ReleaseCapture();
		#endif

		auto prev_top = wxTheApp->GetTopWindow();

		if (auto real_prev_top = dynamic_cast<wxTopLevelWindow*>(prev_top))
			this->SetIcons(real_prev_top->GetIcons());

		wxTheApp->SetTopWindow(this);
		auto res = BaseDialog::ShowModal();
		wxTheApp->SetTopWindow(prev_top);

		return res;
	}

	bool Show(bool show = true) override
	{
		this->CenterOnParent();

		// See https://wx-dev.narkive.com/H4M3aAH7/wxmsw-freeze-with-modal-dialogs-and-popup-menus#post1
		#ifdef __WXMSW__
			// All open menus need to be closed or app will become unresponsive.
			::EndMenu();

			// For same reason release mouse capture.
			// Could happen during drag&drop with notification dialogs.
			::ReleaseCapture();
		#endif

		if (auto real_prev_top = dynamic_cast<wxTopLevelWindow*>(wxTheApp->GetTopWindow()))
			this->SetIcons(real_prev_top->GetIcons());

		return BaseDialog::Show(show);
	}

protected:
	bool wx_validate_recursively_ = (this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY), true);
};

template <typename BaseDialog = wxDialog, typename... Args, std::enable_if_t<std::is_base_of_v<wxDialog, BaseDialog>>* = nullptr>
wxDialogEx(Args &&... args) -> wxDialogEx<BaseDialog>;

namespace fx {

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(wxDialogEx)

}

#endif // DIALOGEX_HPP
