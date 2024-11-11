#ifndef FZ_SERIALIZATION_ARCHIVES_ARGV_HPP
#define FZ_SERIALIZATION_ARCHIVES_ARGV_HPP

#include <libfilezilla/string.hpp>

#include "../serialization.hpp"

#include "xml.hpp"

#include <string_view>

#if ENABLE_FZ_SERIALIZATION_DEBUG
#   include <iostream>
#endif

namespace fz::serialization {

namespace detail {

template <> struct archive_error<argv_input_archive> {
	struct xml_flavour_or_version_mismatch{
		std::string description;
	};

	using suberror_type = std::variant<std::string, xml_flavour_or_version_mismatch>;

	struct type: archive_error_with_suberror<suberror_type> {
		using archive_error_with_suberror::archive_error_with_suberror;

		type(const xml_input_archive::error_t &xml_error);

		std::string description() const;

		bool is_xml_flavour_or_version_mismatch() const { return std::holds_alternative<xml_flavour_or_version_mismatch>(suberror()); }

	private:
		static suberror_type xml2argv_suberror(const xml_input_archive::error_t &xml_error);
	};
};

}

//! The argv input archive.
/*! It's basically a wrapper around a special loader for the xml input archive, which converts options on the command line into an xml tree, using the following strategy.

	1) Each --option identifies a node in the tree whose name is "option".
	2) If --option is followed by a space and a word that doesn't begin with '--', that word is the option's value and becomes the value of the xml node
	   with the option's name.
	3) If --option is followed by a '=' sign, attached to it, and a string, attached to the '=' sign,
	   even if it begin with '--' that string is the option's value and becomes the value of the xml node with the option's name.
	4) If ':=' is used instead of '=', then the string that follows becomes the option's value if and only if that option
	   doesn't already have a value.
	5) If ':=' or '=' is used, then the option's value is taken into consideration even if it's empty.
	6) Options can be nested, using the '.' or '+' symbols. Each nesting level corresponds to a node in the tree that is the child of the node
	   whose name is the same as the option's previous nesting level.
	7) If the '+' symbol is used as a separator between option levels, then the nesting level that follows the '+' symbol is the name of a node that is appended to the list
	   of the siblings nodes.
	8) If '@' is used instead of '.', then what follows is the name of an attribute belonging to the tree node whose name is the word that precedes the '@' symbol.
	9) An option is defined to be "terminal" if it's the deepest one in the nesting hierarchy.
	10) An option that is also an attribute *must* be terminal.
	11) If a given nesting level contains a node with the same name as a terminal option's, then the existing node's value get changed to the option's one,
	   *unless* the node has the fz:can-override attribute set to "false", in which case a new node gets created with the terminal option's name and *appended*
	   to the list of nodes for its nesting level.
	12) Once a terminal option has its value set, the corresponding node also gets the "fz:can-override" attribute set to false.
	13) The net effect of the previous two rules is that an option can be "closed" by declaring that option on the command line,
	   without any associated value: this will set the fz:can-override attribute to false, but will keep its pre-existing value.
	14) Closing an option let's one append to that option's level a sibling of that option. This way, lists/vectors/maps can be built, entirely on the command line.
	15) Options set on the command line also get the "fz:argv" attribute set to true. This is used by the check_for_unhandled_options() method.

	Example:
	\code{.sh}
	program --vec.list.elem 1 --vec.list.elem 2 --vec.list --vec.list.elem 1 --vec.list.elem 2
	\endcode

	Produces the following tree
	\code{.xml}
	<?xml version="1.0"?>
	<filezilla>
		<vec fz:argv="true">
			<list fz:argv="true" fz:can-override="false">
				<elem fz:argv="true" fz:can-override="false">1</elem>
				<elem fz:argv="true" fz:can-override="false">2</elem>
			</list>
			<list fz:argv="true">
				<elem fz:argv="true" fz:can-override="false">1</elem>
				<elem fz:argv="true" fz:can-override="false">2</elem>
			</list>
		</vec>
	</filezilla>
	\endcode
*/
class argv_input_archive: public archive_input<argv_input_archive>, public archive_is_textual  {
	using errors = detail::archive_error<argv_input_archive>;

public:
	struct xml_file {
		native_string file_path;
		bool overridable;
		xml_input_archive::options xml_options = {};
	};

	argv_input_archive(int argc, char *argv[], std::initializer_list<xml_file> xml_backup_list = {}, xml_input_archive::options::verify_mode verify_mode = xml_input_archive::options::verify_mode::ignore, std::vector<fz::native_string> *backups_made = nullptr, bool *verify_errors_ignored = nullptr);

	argv_input_archive(int argc, wchar_t *argv[], std::initializer_list<xml_file> xml_backup_list = {}, xml_input_archive::options::verify_mode verify_mode = xml_input_archive::options::verify_mode::ignore, std::vector<fz::native_string> *backups_made = nullptr, bool *verify_errors_ignored = nullptr);

	argv_input_archive &check_for_unhandled_options();

private:
	struct argv_loader: xml_input_archive::loader {
		argv_loader(int argc, char *argv[], std::initializer_list<xml_file> xml_backup_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored);

		argv_loader(int argc, wchar_t *argv[], std::initializer_list<xml_file> xml_backup_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored);

		native_string_view name() const override;

		xml_input_archive::error_t operator ()(pugi::xml_document &document) override;

	private:
		char **to_utf8(int argc, wchar_t **argv);

		std::vector<std::string> utf8_string_argv_;
		std::vector<char *> utf8_argv_;

		int argc_;
		char **argv_;
		std::vector<xml_file> xml_files_list_;
		xml_input_archive::options::verify_mode verify_mode_;
		std::vector<fz::native_string> *backups_made_;
		bool *verify_errors_ignored_;
	} argv_loader_;

	xml_input_archive xml_;
	inline static std::string anonymous_ = "fz:anonymous";

public:
	// Delegate deserialization to the XML input archive.
	template <typename T>
	friend auto serialize(argv_input_archive &ar, T && v) -> decltype(serialize(std::declval<xml_input_archive &>(), std::forward<T>(v))) {
		serialize(ar.xml_, std::forward<T>(v));
		ar.error(ar.xml_.error());
	}

	template <typename T>
	friend auto load(argv_input_archive &ar, T &&v) -> decltype(load(std::declval<xml_input_archive &>(), std::forward<T>(v))) {
		load(ar.xml_, std::forward<T>(v));
		ar.error(ar.xml_.error());
	}

	template <typename T, typename U>
	friend auto load_minimal(const argv_input_archive &ar, T &&v, const U &u) -> decltype(load_minimal(std::declval<const xml_input_archive &>(), std::forward<T>(v), u)) {
		load_minimal(ar.xml_, std::forward<T>(v), u);
	}

	template <typename T, typename U>
	friend auto load_minimal(const argv_input_archive &ar, T &&v, const U &u, typename T::error_t &argv_error) -> decltype(load_minimal(std::declval<const xml_input_archive &>(), std::forward<T>(v), u, std::declval<xml_input_archive::error_t &>())) {
		xml_input_archive::error_t xml_error;
		load_minimal(ar.xml_, std::forward<T>(v), u, xml_error);
		argv_error = xml_error;
	}
};

struct argv_output_archive: public archive_output<argv_output_archive>, public archive_is_textual {

};

}

FZ_SERIALIZATION_LINK_ARCHIVES(argv_input_archive, argv_output_archive)


#endif // FZ_SERIALIZATION_ARCHIVES_ARGV_HPP
