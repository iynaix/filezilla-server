#include "xml.hpp"

#include "../../preprocessor/str.hpp"
#include "../../util/io.hpp"
#include "../../util/filesystem.hpp"
#include "../../util/overload.hpp"
#include "../../build_info.hpp"

namespace fz::serialization {

namespace detail {

	namespace {

		std::string node_to_path(char initial_sep, pugi::xml_node parent, std::string_view elem) {
			std::string path;

			path.append(elem.rbegin(), elem.rend());

			for (char sep = initial_sep; parent.type() == pugi::node_element; parent = parent.parent(), sep = '.') {
				elem = parent.name();
				path.append(1, sep).append(elem.rbegin(), elem.rend());
			}

			std::reverse(path.begin(), path.end());

			return path;
		}

		static std::string to_string(const pugi::xml_parse_result &result, native_string_view filename) {
			return fz::sprintf("%s, in %s at offset %d.", result.description(), filename, result.offset);
		}

	}

	std::string archive_error<xml_input_archive>::element_error::to_string() const {
		return node_to_path('.', parent, name);
	}

	std::string archive_error<xml_input_archive>::attribute_error::to_string() const {
		return node_to_path('@', parent, name);
	}

	std::string archive_error<xml_input_archive>::type::description() const {
		return std::visit(util::overload {
			[](std::monostate) { return std::string(); },

			[&](const struct element_error &e) {
				if (error_ == ENOENT)
					return fz::sprintf("Missing required element: %s", e.to_string());
				else
					return fz::sprintf("Error in %s: %s", e.to_string(), std::strerror(error_));
			},

			[&](const struct attribute_error &e) {
				if (error_ == ENOENT)
					return fz::sprintf("Missing required attribute: %s", e.to_string());
				else
					return fz::sprintf("Error in %s: %s", e.to_string(), std::strerror(error_));
			},

			[](const struct invalid_xml &e) {
				return fz::sprintf("Invalid XML: %s", e.location);
			},

			[&](const struct custom_error1 &e) {
				return fz::sprintf("Error %d (%s)", error_, e.value);
			},

			[&](const struct flavour_or_version_mismatch &e) {
				return e.description;
			},

			[](const std::string &e) {
				return e;
			},
		}, suberror());
	}

	archive_error<xml_input_archive>::type::type(const pugi::xml_parse_result &result, native_string_view filename)
		: archive_error_with_suberror(
			result.status == pugi::status_file_not_found || result.status == pugi::status_no_document_element
				? ENOENT
				: result.status != pugi::status_ok
					? EINVAL
					: 0,
			  result.status != pugi::status_ok ? suberror_type(mpl::identity<struct invalid_xml>::type{to_string(result, filename)}) : suberror_type()
		)
	{}

}

void xml_output_archive::buffer_saver::write(const void *data, size_t size)
{
	buffer_.append(static_cast<const unsigned char*>(data), size);
}

bool xml_output_archive::buffer_saver::operator()(pugi::xml_document &document, unsigned int flags) {
	document.save(*this, PUGIXML_TEXT("\t"), flags);
	return true;
}

void xml_output_archive::file_saver::write(const void *data, size_t size)
{
	if (!has_error_)
		has_error_ = !util::io::write(file_, data, size);
}

xml_output_archive::file_saver::file_saver(native_string filename, bool write_to_tmp_file_first)
	: filename_(std::move(filename))
	, write_to_tmp_file_first_(write_to_tmp_file_first)
{}

bool xml_output_archive::file_saver::operator()(pugi::xml_document &document, unsigned int flags) {
	//FIXME: this is expensive
	has_error_ = !fz::mkdir(fz::to_native(fz::util::fs::native_path(filename_).parent()), true, fz::mkdir_permissions::cur_user_and_admins);
	if (has_error_)
		return false;

	const auto filename = write_to_tmp_file_first_
			? native_string(filename_).append(fzT(".tmp~"))
			: filename_;

	has_error_ = !file_.open(filename, fz::file::writing, fz::file::empty | fz::file::current_user_and_admins_only);
	if (has_error_)
		return false;

	document.save(*this, PUGIXML_TEXT("\t"), flags);
	file_.close();

	if (write_to_tmp_file_first_) {
		if (!has_error_)
			has_error_ = !fz::rename_file(filename, filename_, false);

		if (has_error_)
			fz::remove_file(filename, false);
	}

	return !has_error_;
}

xml_output_archive::xml_output_archive(xml_output_archive::saver &saver, xml_output_archive::options opts)
	: saver_(saver)
	, opts_{std::move(opts)}
	, num_children_stack_{opts_.stack() ? *opts_.stack() : *new std::stack<unsigned int>}
	, num_children_stack_allocated_{!opts_.stack()}
	, root_node_name_{opts_.root_node_name().c_str()}
{
	if (!opts_.doctype().empty()) {
		auto doctype = document_.prepend_child(pugi::node_doctype);
		doctype.set_value(opts_.doctype().c_str());
	}

	if (opts_.emit_prolog()) {
		auto prolog = document_.prepend_child(pugi::node_declaration);
		prolog.append_attribute("version") = "1.0";
		prolog.append_attribute("encoding") = "UTF-8";
		prolog.append_attribute("standalone") = "yes";
	}

	set_next_names({&root_node_name_, 0, false});
	open_node();

	attribute(std::string("https://filezilla-project.org"), "xmlns:fz");
	attribute(std::string("https://filezilla-project.org"), "xmlns");
	attribute(std::string("http://www.w3.org/2001/XMLSchema-instance"), "xmlns:xsi");

	if (opts_.emit_version()) {
		attribute(toString<std::string>(fz::build_info::flavour), "fz:product_flavour");
		attribute(toString<std::string>(fz::build_info::version), "fz:product_version");
	}
}

xml_output_archive::~xml_output_archive()
{
	if (!error_) {
		close_node();
		unsigned int flags = opts_.must_indent() ? pugi::format_indent : pugi::format_raw;
		flags             |= opts_.emit_prolog() ? 0 : pugi::format_no_declaration;

		bool save_result = saver_(document_, flags);
		if (opts_.save_result())
			*opts_.save_result() = save_result;
	}

	if (num_children_stack_allocated_)
		delete &num_children_stack_;
}

xml_input_archive::buffer_loader::buffer_loader(buffer &buffer, bool parse_in_place)
	: buffer_(buffer)
	, parse_in_place_(parse_in_place)
{}

native_string_view xml_input_archive::buffer_loader::name() const
{
	return fzT("<document>");
}

xml_input_archive::error_t xml_input_archive::buffer_loader::operator()(pugi::xml_document &document) {
	if (parse_in_place_)
		return { document.load_buffer_inplace(buffer_.get(), buffer_.size(), pugi::parse_default | pugi::parse_trim_pcdata), fzT("<document>") };

	return { document.load_buffer(buffer_.get(), buffer_.size(), pugi::parse_default | pugi::parse_trim_pcdata), fzT("<document>") };
}

xml_input_archive::file_loader::file_loader(native_string filename)
	: filename_(std::move(filename))
{}

native_string_view xml_input_archive::file_loader::name() const
{
	return filename_;
}

xml_input_archive::error_t xml_input_archive::file_loader::operator()(pugi::xml_document &document)
{
	return { document.load_file(filename_.c_str(), pugi::parse_default | pugi::parse_trim_pcdata), filename_ };
}

namespace {

int get_errno() {
#ifdef FZ_WINDOWS
  return EIO;
#else
  return errno;
#endif
}

}

xml_input_archive::xml_input_archive(xml_input_archive::loader &loader, xml_input_archive::options opts)
	: opts_{opts}
	, root_node_name_{opts_.root_node_name().c_str()}
{
	if (auto err = loader(document_); err) {
		error(std::move(err));
		return;
	}

	cur_node_ = document_;
	cur_child_ = first_element_child(cur_node_);

	trashcan_ = document_.append_child();

	set_next_names({&root_node_name_, 0, false});
	open_node();

	if (error_) {
		return;
	}

	std::string product_flavour_s = toString<std::string>(build_info::flavour);
	std::string product_version_s = build_info::version;

	optional_attribute(product_flavour_s, "fz:product_flavour").
	optional_attribute(product_version_s, "fz:product_version");

	if (error_)
		return;

	native_string backup_dir_name;
	auto error_str = version_check(product_flavour_s, product_version_s, loader.name(), backup_dir_name);
	if (error_str.empty())
		return;

	if (opts_.verify_version() == options::verify_mode::ignore)
	{
		if (opts_.verify_error_ignored())
			*opts_.verify_error_ignored() = true;

		return;
	}

	if (backup_dir_name.empty()) {
		// Unrecoverable error
		error({EINVAL, error_str});
		return;
	}

	if (opts_.verify_version() == options::verify_mode::error) {
		error({EINVAL, errors::flavour_or_version_mismatch{error_str}});
		return;
	}

	if (opts_.verify_version() == options::verify_mode::backup) {
		util::fs::native_path docpath = loader.name();

		if (!docpath.is_absolute()) {
			// Unrecoverable error
			error({EINVAL, error_str});
			return;
		}

		auto dot_backups_dir = docpath.parent() / fzT(".backups");
		if (auto res = fz::mkdir(dot_backups_dir, false, fz::mkdir_permissions::cur_user_and_admins); !res) {
			error({get_errno(), fz::sprintf("Failed making directory `%s`. Result: %d.", dot_backups_dir.str(), res.error_)});
			return;
		}

		auto backup_dir = dot_backups_dir / backup_dir_name;
		if (auto res = fz::mkdir(backup_dir, false, fz::mkdir_permissions::cur_user_and_admins); !res) {
			error({get_errno(), fz::sprintf("Failed making directory %s. Result: %d.", backup_dir.str(), res.error_)});
			return;
		}

		int ioerr;
		native_string backup = backup_dir / docpath.base();
		if (!util::io::copy(docpath, backup, fz::file::current_user_and_admins_only, &ioerr)) {
			error({ioerr, fz::sprintf("Failed making backup copy of %s.", docpath)});
			return;
		}

		if (opts_.backup_made())
			*opts_.backup_made() = backup;

		return;
	}

	error({EINVAL, fz::sprintf("Unknown value for the verify_version option: %d", int(opts_.verify_version()))});
}

xml_input_archive::~xml_input_archive()
{
	if (!error_ && document_.document_element())
		close_node();
}

pugi::xml_node xml_input_archive::root_node() {
	return document_.child(root_node_name_);
}

build_info::version_info xml_input_archive::get_product_version() const
{
	return product_version_;
}

std::string xml_input_archive::version_check(std::string_view flavour, std::string_view version, native_string_view docname, native_string &backup_dir_name)
{
	build_info::flavour_type f;
	if (!convert(flavour, f))
		return fz::sprintf("Invalid product flavour: \"%s\". In %s.", flavour, docname);

	product_version_ = version;
	if (!product_version_)
		return fz::sprintf("Invalid product version: \"%s\". In %s.", version, docname);

	// At this point we know we've got syntactically valid flavour and version
	backup_dir_name = (fz::to_native(flavour) += fzT("-")) += fz::to_native(fz::replaced_substrings(version, ".", "_"));

	if (f != build_info::flavour)
		return fz::sprintf("Product flavour mismatch: expected product flavour \"%s\", got \"%s\". In %s.", build_info::flavour, flavour, docname);

	if (product_version_ > build_info::version)
		return fz::sprintf("Product version mismatch: version must be <= than \"%s\", got \"%s\" instead. In %s.", build_info::version, version, docname);

	return {};
}

bool convert(std::string_view s, xml_input_archive::options::verify_mode &v)
{
	using vm = xml_input_archive::options::verify_mode;

	if (s == "ignore") {
		v = vm::ignore;
		return true;
	}

	if (s == "error") {
		v = vm::error;
		return true;
	}

	if (s == "backup") {
		v = vm::backup;
		return true;
	}

	return false;
}

xml_output_archive::saver::~saver() = default;

}
