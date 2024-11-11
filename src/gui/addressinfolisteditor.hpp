#ifndef ADDRESSINFOLISTEDITOR_H
#define ADDRESSINFOLISTEDITOR_H

#include <vector>
#include <functional>
#include <map>

#include <wx/panel.h>
#include <wx/weakref.h>

#include "../filezilla/tcp/address_info.hpp"
#include "glue.hpp"

class wxButton;
class wxTextCtrl;
class IntegralEditor;
class wxChoice;

class AddressInfoListEditor: public wxPanel
{
	using validator_t = std::function<wxString (const wxString &address, unsigned int port)>;
	class Grid;
	class Table;

public:
	class ListInfo final
	{
	public:
		template <typename AddressInfo>
		struct Info
		{
			wxString proto_name;
			std::function<AddressInfo(fz::tcp::address_info &&ai)> convert;
			std::function<bool(const AddressInfo &ai)> can_convert;
		};

		template <typename AddressInfo>
		ListInfo(std::vector<AddressInfo> &ai_list, std::initializer_list<Info<AddressInfo>> info);

		template <typename AddressInfo>
		ListInfo(std::vector<AddressInfo> &ai_list, const wxString &proto_name);

	private:
		friend Table;

		std::function<void()> clear_list_;
		std::function<void(Table &t)> append_list_to_table_;
		std::vector<std::pair<wxString /*proto name*/, std::function<void(fz::tcp::address_info &&)>> /*list appender*/> append_address_to_list_;
	};

	AddressInfoListEditor() = default;

	bool Create(wxWindow *parent, bool edit_proto = true);

	//! Sets the lists into the editor.
	//! It *doesn't* take ownership of the lists.
	void SetLists(std::initializer_list<ListInfo> lists);
	void SetAddressAndPortValidator(validator_t validator);
	wxString GetMatchingAddress(const wxString &address, unsigned int port, bool any_is_equivalent);
	unsigned int GetFirstPortInRange(unsigned int min, unsigned int max);

private:
	Grid *grid_;
	Table *table_;
	wxButton *remove_button_;
	wxButton *add_button_;

	static void append_to_table(Table &t, const std::string &address, unsigned int port, const wxString &proto_name);
};

template <typename AddressInfo>
AddressInfoListEditor::ListInfo::ListInfo(std::vector<AddressInfo> &ai_list, std::initializer_list<Info<AddressInfo>> info)
	: clear_list_([&ai_list] { ai_list.clear(); })
{
	std::vector<std::pair<wxString, std::function<bool(const AddressInfo &ai)>>> can_convert_list;

	for (auto &i: info) {
		if (i.proto_name.empty() || !i.convert || !i.can_convert)
			continue;

		auto append = [&ai_list, convert = i.convert](fz::tcp::address_info &&ai) {
			ai_list.push_back(convert(std::move(ai)));
		};

		append_address_to_list_.emplace_back(i.proto_name, std::move(append));
		can_convert_list.emplace_back(i.proto_name, i.can_convert);
	}

	append_list_to_table_ = [&ai_list, can_convert_list = std::move(can_convert_list)](Table &t) {
		for (auto &ai: ai_list) {
			for (auto &[proto_name, can_convert]: can_convert_list) {
				if (can_convert(ai)) {
					append_to_table(t, ai.address, ai.port, proto_name);
				}
			}
		}
	};
}

template <typename AddressInfo>
AddressInfoListEditor::ListInfo::ListInfo(std::vector<AddressInfo> &ai_list, const wxString &proto_name)
	: ListInfo(ai_list, { {
		proto_name,
		[](fz::tcp::address_info &&ai) {
			return AddressInfo{ std::move(ai) };
		},
		[](const AddressInfo &) {
			return true;
		}
	}})
{}

#endif // ADDRESSINFOLISTEDITOR_H
