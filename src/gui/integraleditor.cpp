#include <libfilezilla/glue/wx.hpp>

#include "integraleditor.hpp"

#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/valtext.h>
#include <wx/log.h>
#include <wx/stattext.h>
#include <wx/spinbutt.h>

#include "glue.hpp"
#include "locale.hpp"

#include "../filezilla/util/integral_ops.hpp"

#include "helpers.hpp"

bool IntegralEditor::Create(wxWindow *parent, const wxString &unit, std::uintmax_t scale, long style, wxWindowID winid, const wxPoint &pos, const wxSize &size, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, wxTAB_TRAVERSAL | wxNO_BORDER, name))
		return false;

	scale_ = scale;
	style_ = style;

	text_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, (unit.empty() ? wxTE_LEFT : wxTE_RIGHT)|(style & wxBORDER_MASK));

	if (style & IE_WITH_SPIN) {
		struct Spin: wxSpinButton {
			using wxSpinButton::wxSpinButton;

			bool AcceptsFocus() const override
			{
				return false;
			}

			bool AcceptsFocusFromKeyboard() const override
			{
				return false;
			}
		};

		spin_ctrl_ = new Spin(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL | wxSP_WRAP);
	}

	if (spin_ctrl_) {
		auto s = spin_ctrl_->GetMinSize();
		s.SetHeight(text_ctrl_->GetSize().GetHeight());
		spin_ctrl_->SetMinSize(s);
	}

	wxHBox(this, 0, 0) = {
		wxSizerFlags(1).Expand() >>= text_ctrl_,
		wxSizerFlags(0).Expand() >>= spin_ctrl_ ,
		unit_ctrl_ = new wxStaticText(this, wxID_ANY, unit) |[&](auto p) {
			if (unit.empty())
				p->Hide();
		}
	};

	Bind(wxEVT_CHILD_FOCUS, [&](wxChildFocusEvent &) {
		wxChildFocusEvent ev(this);
		ev.SetEventObject(this);

		GetParent()->GetEventHandler()->ProcessEvent(ev);
	});

	text_ctrl_->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &ev) {
		auto uni = ev.GetUnicodeKey();
		auto code = ev.GetKeyCode();
		auto mod = ev.GetModifiers();

		if ((mod & wxMOD_ALT) == wxMOD_ALT) {
			ev.Skip();
			return;
		}

		if (ref_.has_map(text_ctrl_->GetValue()))
			goto check_up_down_arrows;

		if (uni == L'-') {
			if (!ref_.is_signed())
				return;

			// Only allow '-' sign at the beginning.
			if (text_ctrl_->GetInsertionPoint() != 0)
				return;

			// But only if there's not already one.
			if (const auto &s = text_ctrl_->GetValue(); s.size() > 0 && s[0] == L'-')
				return;

			ev.Skip();
		}
		else
		if (uni >= L'0' && uni <= L'9')
			ev.Skip();
		else
		if (code == 'A' || code == 'X' || code == 'V' || code == 'C') {
			if (ev.ControlDown())
				ev.Skip();
		}
		else
		if (code >= WXK_NUMPAD0 && code <= WXK_NUMPAD9)
			ev.Skip();
		else
		if (code == WXK_DELETE || code == WXK_BACK || code == WXK_RETURN || code == WXK_ESCAPE || code == WXK_TAB || code == WXK_NUMPAD_TAB)
			ev.Skip();
		else
		if (code == WXK_LEFT || code == WXK_RIGHT || code == WXK_NUMPAD_LEFT || code == WXK_NUMPAD_RIGHT)
			ev.Skip();
		else
		if (code == WXK_HOME || code == WXK_NUMPAD_HOME || code == WXK_END || code == WXK_NUMPAD_END)
			ev.Skip();
		else
		if (code == WXK_NUMLOCK)
			ev.Skip();
		else check_up_down_arrows:
		if (!spin_ctrl_) {
			if (code == WXK_UP || code == WXK_NUMPAD_UP)
				ev.Skip();
			else
			if (code == WXK_DOWN || code == WXK_NUMPAD_DOWN)
				ev.Skip();
		}
		else {
			std::uintmax_t amount = 1;

			if (mod & wxMOD_CONTROL)
				amount *= 10;
			if (mod & wxMOD_SHIFT) {
				amount *= 100;
			}

			if (code == WXK_UP || code == WXK_NUMPAD_UP)
				modify(true, amount);
			else
			if (code == WXK_DOWN || code == WXK_NUMPAD_DOWN)
				modify(false, amount);
		}
	});

	if (spin_ctrl_) {
		spin_ctrl_->Bind(wxEVT_SPIN_UP, [this](wxSpinEvent &) {
			modify(true, 1);
		});

		spin_ctrl_->Bind(wxEVT_SPIN_DOWN, [this](wxSpinEvent &) {
			modify(false, 1);
		});
	}

	SetRef(nullptr);

	return true;
}

void IntegralEditor::SetRef(ref_t ref)
{
	if (ref) {
		auto res = ref.from_string(ref.to_string(scale_), scale_);
		if (res == fz::util::integral_op_result::underflow)
			ref.set_to_min();
		else
		if (res == fz::util::integral_op_result::overflow)
			ref.set_to_max();
		else
		if (!res)
			ref = {};
	}

	ref_ = std::move(ref);

	TransferDataToWindow();

	Event::Changed.Process(this, this);
}

bool IntegralEditor::SetUnit(const wxString &name, uintmax_t scale)
{
	scale_ = scale;

	unit_ctrl_->SetLabel(name);
	unit_ctrl_->Show(!name.empty());
	unit_ctrl_->Refresh();
	text_ctrl_->SetWindowStyle(name.empty() ? wxTE_LEFT : wxTE_RIGHT);

	Layout();

	return true;
}

void IntegralEditor::AutoComplete(const wxArrayString &array)
{
	wxArrayString filtered;

	auto old = ref_.to_string(scale_);

	for (const auto &a: array) {
		if (ref_.from_string(a, scale_))
			filtered.push_back(a);
	}

	ref_.from_string(old, scale_);

	text_ctrl_->AutoComplete(filtered);
}

bool IntegralEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	if (ref_) {
		if (!ref_.from_string(text_ctrl_->GetValue(), scale_)) {
			auto old = ref_.to_string(scale_);

			auto min = ref_.set_to_min().to_string(scale_);
			auto max = ref_.set_to_max().to_string(scale_);

			ref_.from_string(old, scale_);

			text_ctrl_->SetFocus();
			wxMsg::Error(_S("Wrong value.\n\nMust be not less than %s and not greater than %s."), min, max);
			return false;
		}
	}

	return true;
}

bool IntegralEditor::TransferDataToWindow()
{
	if (!wxPanel::TransferDataToWindow())
		return false;

	if (ref_) {
		text_ctrl_->SetValue(ref_.to_string(scale_));
		text_ctrl_->Enable();
		text_ctrl_->SetInsertionPointEnd();

		if (spin_ctrl_)
			spin_ctrl_->Enable();
	}
	else {
		text_ctrl_->Clear();
		text_ctrl_->Disable();

		if (spin_ctrl_)
			spin_ctrl_->Disable();
	}

	return true;
}

void IntegralEditor::modify(bool up, uintmax_t amount)
{
	auto text = text_ctrl_->GetValue();
	if (text.empty())
		text = ref_.set_to_min().to_string(scale_);

	auto result = ref_.from_string(text, scale_);

	if (result) {
		if (up) {
			if (!ref_.increment(amount, scale_))
				result = fz::util::integral_op_result::overflow;
		}
		else
		if (!ref_.decrement(amount, scale_))
			result = fz::util::integral_op_result::underflow;
	}

	if (!result)
		wxBell();

	if (result == fz::util::integral_op_result::underflow) {
		if (style_ & IE_WRAPAROUND && !up)
			ref_.set_to_max();
		else
			ref_.set_to_min();
	}
	else
	if (result == fz::util::integral_op_result::overflow) {
		if (style_ & IE_WRAPAROUND && up)
			ref_.set_to_min();
		else
			ref_.set_to_max();
	}

	SetRef(std::move(ref_));
}

template<typename T>
bool IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::increment(uintmax_t amount, uintmax_t scale)
{
	return fz::util::increment(this->get(), amount, scale, min_, max_);
}

template<typename T>
bool IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::decrement(uintmax_t amount, uintmax_t scale)
{
	return fz::util::decrement(this->get(), amount, scale, min_, max_);
}

template<typename T>
fz::util::integral_op_result IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::from_string(const wxString &s, uintmax_t scale)
{
	auto it = std::find_if(map_.begin(), map_.end(), [&s] (auto &p) {
		return s == p.second;
	});

	if (it != map_.end()) {
		this->get() = it->first;
		return fz::util::integral_op_result::ok;
	}

	return fz::util::convert(s, this->get(), scale, min_, max_);
}

template<typename T>
wxString IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::to_string(uintmax_t scale) const
{
	auto it = std::find_if(map_.begin(), map_.end(), [&] (auto &p) {
		return this->get() / scale == p.first / scale;
	});

	if (it != map_.end())
		return it->second;

	wxString s;

	fz::util::convert(this->get(), s, scale);

	return s;
}

template<typename T>
bool IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::has_map(const wxString &s) const
{
	auto it = std::find_if(map_.begin(), map_.end(), [&s] (auto &p) {
		return s == p.second;
	});

	return it != map_.end();
}

template<typename T>
void IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::set_to_min()
{
	this->get() = min_;
}

template<typename T>
void IntegralEditor::limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>::set_to_max()
{
	this->get() = max_;
}

template<typename Dummy>
bool IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::increment(uintmax_t, uintmax_t)
{
	return false;
}

template<typename Dummy>
bool IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::decrement(uintmax_t, uintmax_t)
{
	return false;
}

template<typename Dummy>
fz::util::integral_op_result IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::from_string(const wxString &, uintmax_t)
{
	return fz::util::integral_op_result::bad_value;
}

template<typename Dummy>
wxString IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::to_string(uintmax_t) const
{
	return wxEmptyString;
}

template<typename Dummy>
void IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::set_to_min()
{}

template<typename Dummy>
void IntegralEditor::limited_ref_t<std::nullptr_t, Dummy>::set_to_max()
{}

template<typename Dummy>
bool IntegralEditor::limited_ref_t<fz::duration, Dummy>::increment(uintmax_t amount, uintmax_t scale)
{
	auto value = this->get().get_milliseconds();
	if (fz::util::increment(value, amount, scale, min_.get_milliseconds(), max_.get_milliseconds())) {
		this->get() = fz::duration::from_milliseconds(value);
		return true;
	}

	return false;
}

template<typename Dummy>
bool IntegralEditor::limited_ref_t<fz::duration, Dummy>::decrement(uintmax_t amount, uintmax_t scale)
{
	auto value = this->get().get_milliseconds();
	if (fz::util::decrement(value, amount, scale, min_.get_milliseconds(), max_.get_milliseconds())) {
		this->get() = fz::duration::from_milliseconds(value);
		return true;
	}

	return false;
}

template<typename Dummy>
fz::util::integral_op_result IntegralEditor::limited_ref_t<fz::duration, Dummy>::from_string(const wxString &s, uintmax_t scale)
{
	int64_t value = 0;
	auto result = fz::util::convert(s, value, scale, min_.get_milliseconds(), max_.get_milliseconds());

	if (result)
		this->get() = fz::duration::from_milliseconds(value);

	return result;
}

template<typename Dummy>
void IntegralEditor::limited_ref_t<fz::duration, Dummy>::set_to_min()
{
	this->get() = min_;
}

template<typename Dummy>
void IntegralEditor::limited_ref_t<fz::duration, Dummy>::set_to_max()
{
	this->get() = max_;
}

template<typename Dummy>
wxString IntegralEditor::limited_ref_t<fz::duration, Dummy>::to_string(uintmax_t scale) const
{
	wxString ret;
	int64_t value = this->get().get_milliseconds();
	fz::util::convert(value, ret, scale);
	return ret;
}

IntegralEditor::ref_t::operator bool() const
{
	return std::get_if<limited_ref_t<std::nullptr_t>>(this) == nullptr;
}

bool IntegralEditor::ref_t::increment(uintmax_t amount, uintmax_t scale)
{
	return std::visit([&](auto &ref){
		return ref.increment(amount, scale);
	}, static_cast<variant&>(*this));
}

bool IntegralEditor::ref_t::decrement(uintmax_t amount, uintmax_t scale)
{
	return std::visit([&](auto &ref){
		return ref.decrement(amount, scale);
	}, static_cast<variant&>(*this));
}

fz::util::integral_op_result IntegralEditor::ref_t::from_string(const wxString &s, uintmax_t scale)
{
	return std::visit([&](auto &ref){
		return ref.from_string(s, scale);
	}, static_cast<variant&>(*this));
}

wxString IntegralEditor::ref_t::to_string(uintmax_t scale) const
{
	return std::visit([&](auto &ref){
		return ref.to_string(scale);
	}, static_cast<const variant&>(*this));
}

IntegralEditor::ref_t &IntegralEditor::ref_t::set_to_min()
{
	std::visit([&](auto &ref){
		return ref.set_to_min();
	}, static_cast<variant&>(*this));

	return *this;
}

IntegralEditor::ref_t &IntegralEditor::ref_t::set_to_max()
{
	std::visit([&](auto &ref){
		return ref.set_to_max();
	}, static_cast<variant&>(*this));

	return *this;
}

bool IntegralEditor::ref_t::is_signed() const
{
	return std::visit([&](auto &ref){
		return ref.is_signed();
	}, static_cast<const variant&>(*this));
}

bool IntegralEditor::ref_t::has_map(const wxString &s) const
{
	return std::visit([&](auto &ref){
		return ref.has_map(s);
	}, static_cast<const variant&>(*this));
}

bool IntegralEditor::enum_wrapper::increment(uintmax_t amount, uintmax_t scale)
{
	return increment_(*this, amount, scale);
}

bool IntegralEditor::enum_wrapper::decrement(uintmax_t amount, uintmax_t scale)
{
	return decrement_(*this, amount, scale);
}

fz::util::integral_op_result IntegralEditor::enum_wrapper::from_string(const wxString &s, uintmax_t scale)
{
	return from_string_(*this, s, scale);
}

wxString IntegralEditor::enum_wrapper::to_string(uintmax_t scale) const
{
	return to_string_(*this, scale);
}

void IntegralEditor::enum_wrapper::set_to_min()
{
	return set_to_min_(*this);
}

void IntegralEditor::enum_wrapper::set_to_max()
{
	return set_to_max_(*this);
}

bool IntegralEditor::enum_wrapper::is_signed() const
{
	return is_signed_(*this);
}

bool IntegralEditor::enum_wrapper::has_map(const wxString &s) const
{
	auto it = std::find_if(map_.begin(), map_.end(), [&s] (auto &p) {
		return s == p.second;
	});

	return it != map_.end();
}
