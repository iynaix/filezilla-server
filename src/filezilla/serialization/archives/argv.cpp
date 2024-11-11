#include "argv.hpp"
#include "../../util/overload.hpp"

namespace fz::serialization {

namespace detail {

	archive_error<argv_input_archive>::type::type(const xml_input_archive::error_t &xml_error)
		: archive_error_with_suberror(xml_error.value(), xml2argv_suberror(xml_error))
	{}

	std::string archive_error<argv_input_archive>::type::description() const
	{
		return std::visit(util::overload{
			[](const std::string &e) { return e; },
			[](const xml_flavour_or_version_mismatch &e) { return e.description;}
		}, suberror());
	}

	archive_error<argv_input_archive>::type::suberror_type archive_error<argv_input_archive>::type::xml2argv_suberror(const xml_input_archive::error_t &xml_error) {
		using xml_errors = archive_error<xml_input_archive>;

		return std::visit(util::overload {
			[&](const struct xml_errors::element_error &e) -> suberror_type {
				if (xml_error == ENOENT)
					return fz::sprintf("Missing required option: --%s", e.to_string().substr(sizeof("filezilla")));

				return archive_error<xml_input_archive>::type(xml_error, e).description();
			},

			[&](const struct xml_errors::attribute_error &e) -> suberror_type {
				if (xml_error == ENOENT)
					return fz::sprintf("Missing required option: --%s", e.to_string().substr(sizeof("filezilla")));

				return archive_error<xml_input_archive>::type(xml_error, e).description();
			},

			[&](const struct xml_errors::custom_error1 &e) -> suberror_type{
				return fz::sprintf("Invalid option: %s", e.value);
			},

			[&](const struct xml_errors::flavour_or_version_mismatch &e) -> suberror_type {
				return xml_flavour_or_version_mismatch{e.description};
			},

			[&](const auto &e) -> suberror_type {
				return archive_error<xml_input_archive>::type(xml_error, e).description();
			},
		}, xml_error.suberror());
	}
}

argv_input_archive::argv_input_archive(int argc, char *argv[], std::initializer_list<argv_input_archive::xml_file> xml_backup_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored)
	: argv_loader_{argc, argv, std::move(xml_backup_list), verify_mode, backups_made, verify_errors_ignored}
	, xml_{argv_loader_}
{
	error(xml_.error());
}

argv_input_archive::argv_input_archive(int argc, wchar_t *argv[], std::initializer_list<argv_input_archive::xml_file> xml_backup_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored)
	: argv_loader_{argc, argv, std::move(xml_backup_list), verify_mode, backups_made, verify_errors_ignored}
	, xml_{argv_loader_}
{
	error(xml_.error());
}

argv_input_archive &argv_input_archive::check_for_unhandled_options() {
	if (error())
		return *this;

	if (auto cur = xml_input_archive::first_element_child(xml_.root_node(), "fz:argv"); cur) {
		thread_local std::string reconstructed_option_name;

		if (std::string_view(cur.name()) == anonymous_) {
			reconstructed_option_name.append(cur.child_value());
		}
		else {
			reconstructed_option_name = "--";
			reconstructed_option_name.append(cur.name());

			for (auto next = xml_input_archive::first_element_child(cur); next; cur = next, next = xml_input_archive::first_element_child(next)) {
				reconstructed_option_name.append(".").append(next.name());
			}

			if (auto value = cur.child_value(); value[0])
				reconstructed_option_name.append("=").append(value);
		}

		error({EINVAL, fz::sprintf("Unhandled option: %s", reconstructed_option_name.c_str())});
	}

	return *this;
}

argv_input_archive::argv_loader::argv_loader(int argc, char *argv[], std::initializer_list<argv_input_archive::xml_file> xml_files_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored)
	: argc_{argc - (argc > 0)}
	, argv_{argv + (argc > 0)}
	, xml_files_list_(xml_files_list)
	, verify_mode_(verify_mode)
	, backups_made_(backups_made)
	, verify_errors_ignored_(verify_errors_ignored)
{}

argv_input_archive::argv_loader::argv_loader(int argc, wchar_t *argv[], std::initializer_list<argv_input_archive::xml_file> xml_files_list, xml_input_archive::options::verify_mode verify_mode, std::vector<fz::native_string> *backups_made, bool *verify_errors_ignored)
	: argc_{argc - (argc > 0)}
	, argv_{to_utf8(argc - (argc > 0), argv + (argc > 0))}
	, xml_files_list_(xml_files_list)
	, verify_mode_(verify_mode)
	, backups_made_(backups_made)
	, verify_errors_ignored_(verify_errors_ignored)
{}

native_string_view argv_input_archive::argv_loader::name() const
{
	static thread_local native_string paths;

	paths = fzT("<command line>");

	for (const auto &f: xml_files_list_)
		paths.append(fzT(";")).append(f.file_path);

	return paths;
}

xml_input_archive::error_t argv_input_archive::argv_loader::operator ()(pugi::xml_document &document) {
	pugi::xml_node root = document.append_child();
	root.set_name("filezilla");

	if (!xml_files_list_.empty()) {
		if (backups_made_)
			backups_made_->clear();

		pugi::xml_parse_result result;

		for (auto &file: xml_files_list_) {
			native_string backup;
			bool verify_error_ignored{};
			xml_input_archive::file_loader file_loader(file.file_path);
			xml_input_archive ar(file_loader, file.xml_options.verify_version(verify_mode_).backup_made(&backup).verify_error_ignored(&verify_error_ignored));

			if (!ar) {
				if (ar.error().value() != ENOENT)
					return ar.error();

				continue;
			}

			if (backups_made_ && !backup.empty())
				backups_made_->push_back(std::move(backup));

			if (verify_errors_ignored_)
				*verify_errors_ignored_ |= verify_error_ignored;

			for (auto child: ar.root_node().children()) {
				if (!file.overridable && !child.attribute("fz:can-override"))
					child.append_attribute("fz:can-override").set_value(false);

				root.append_copy(child);
			}
		}
	}

	std::string_view option;
	std::string_view attribute;
	bool only_if_nonexisting = false;
	bool force_value = false;
	bool append = false;

	// Postcondition: option.empty() == true;
	auto process_option_and_value = [&option, &attribute, &root, &only_if_nonexisting, &force_value, &append](const std::string_view value)
	{
		pugi::xml_node cur_node = root;
		std::string_view fragment;

		auto update_cur_node = [&cur_node](const std::string &name, bool only_if_non_existing = false, bool append = false) {
			if (!cur_node)
				return;

			auto && children = cur_node.children(name.c_str());
			if (only_if_non_existing && children.begin() != children.end()) {
				cur_node = {};
				return;
			}

			pugi::xml_node overridable;
			for (auto child: children) {
				auto attr = child.attribute("fz:can-override");

				if (append) {
					if (!attr) {
						attr = child.append_attribute("fz:can-override");
					}
					attr.set_value(false);
				}
				else
				if (attr.as_bool(true)) {
					overridable = child;
					break;
				}
			}

			if (overridable)
				cur_node = overridable;
			else {
				cur_node = cur_node.append_child();
				cur_node.append_attribute("fz:argv").set_value(true);

				cur_node.set_name(name.empty() ? anonymous_.c_str() : name.c_str());
			}
		};

		bool append = false;
		for (util::parseable_range r(option);;) {
			bool next_append = false;

			if (parse_until_lit(r, fragment, {'.', '+'}, true)) {
				char sep;
				if (!parse_lit(r, sep)) {
					option = {};
					break;
				}
				else {
					next_append = sep == '+';
				}
			}

			update_cur_node(std::string(fragment), false, append);
			append = next_append;
		}

		update_cur_node(std::string(fragment), only_if_nonexisting && attribute.empty(), append);
		if (!cur_node)
			return;

		if (auto value_str = std::string(value); attribute.empty()) {
			if (!cur_node.attribute("fz:can_override"))
				cur_node.append_attribute("fz:can-override").set_value("false");

			if (force_value || !value_str.empty()) {
				for (auto n = cur_node.first_child(); n;) {
					auto next = n.next_sibling();
					cur_node.remove_child(n);
					n = next;
				}

				cur_node.append_child(pugi::node_pcdata).set_value(value_str.c_str());
			}
		}
		else {
			auto attr_str = std::string(attribute);
			auto cur_attr = cur_node.attribute(attr_str.c_str());

			if (only_if_nonexisting && cur_attr)
				return;

			if (!cur_attr)
				cur_attr = cur_node.append_attribute(attr_str.c_str());
			cur_attr.set_value(value_str.c_str());

			attribute = {};
		}
	};

	static constexpr auto extract_parts = [](std::string_view arg, std::string_view &option, std::string_view &attribute, std::string_view &value, bool &only_if_nonexisting, bool &force_value, bool &append) {
		auto it = arg.begin(), end = arg.end();

		auto match_segment = [&it, &end]() {
			static constexpr auto is_alpha = [](char ch) { return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z'); };
			static constexpr auto is_num   = [](char ch) { return ('0' <= ch && ch <= '9'); };
			static constexpr auto is_w     = [](char ch) { return is_alpha(ch) || is_num(ch) || ch == '_' || ch == '-'; };

			if (it != end && is_w(*it) && !is_num(*it)) {
				while (++it != end && is_w(*it));

				return true;
			}

			return false;
		};

		auto match_lit = [&it, &end](auto... chs) {
			if (it != end && ((*it == chs) || ...)) {
				++it;
				return true;
			}

			return false;
		};

		if (it == end) {
			option = {};
			return true;
		}

		if (!match_segment())
			return false;

		while (match_lit('.', '+') && match_segment());

		option = arg.substr(0, std::size_t(it-arg.begin()));
		arg.remove_prefix(option.size());
		it = arg.begin();

		if (match_lit('@')) {
			if (!match_segment())
				return false;

			attribute = arg.substr(1, std::size_t(it-arg.begin())-1);
			arg.remove_prefix(1+attribute.size());
			it = arg.begin();
		}

		only_if_nonexisting = false;
		append = false;

		if (match_lit(':')) {
			if (match_lit('=')) {
				value = arg.substr(2);
				only_if_nonexisting = true;
				force_value = true;
			}
			else {
				return false;
			}
		}
		else
		if (match_lit('=')) {
			value = arg.substr(1);
			force_value = true;
		}
		else {
			force_value = false;

			if (it != end) {
				return false;
			}
		}

		return true;
	};

	for (int idx = 0; idx < argc_; ++idx) {
		auto arg = std::string_view(argv_[idx]);
		auto is_option = arg.substr(0, 2) == "--";

		if (is_option) {
			if (!option.empty()) {
				// In case we've already got an option, it means it has to be intended as a boolean toggle set to true.
				process_option_and_value({});
			}

			arg.remove_prefix(2);

			if (std::string_view value; extract_parts(arg, option, attribute, value, only_if_nonexisting, force_value, append)) {
				if (option.empty()) {
					option = "fz:unhandled-args";
					process_option_and_value(fz::to_string(argc_-idx-1));
					break;
				}

				if (force_value || !value.empty())
					process_option_and_value(value);
			}
			else {
				return {EINVAL, detail::archive_error<xml_input_archive>::custom_error1{arg}};
			}
		}
		else {
			process_option_and_value(arg);
		}
	}

	if (!option.empty()) {
		// In case we've already got an option, it means it has to be intended as a boolean toggle set to true.
		process_option_and_value({});
	}

#if ENABLE_FZ_SERIALIZATION_DEBUG
	document.save(std::cerr);
#endif

	return {0};
}

char **argv_input_archive::argv_loader::to_utf8(int argc, wchar_t **argv)
{
	utf8_argv_.reserve((size_t)argc);
	utf8_string_argv_.reserve((size_t)argc);

	for (int i = 0; i < argc; ++i) {
		auto &s = utf8_string_argv_.emplace_back(fz::to_utf8(argv[i]));
		utf8_argv_.push_back(s.data());
	}

	return utf8_argv_.data();
}



}
