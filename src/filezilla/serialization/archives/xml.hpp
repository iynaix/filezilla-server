#ifndef FZ_SERIALIZATION_ARCHIVES_XML_HPP
#define FZ_SERIALIZATION_ARCHIVES_XML_HPP

#include <stack>
#include <cstring>
#include <variant>

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/encode.hpp>
#include <libfilezilla/format.hpp>
#include <libfilezilla/file.hpp>

#include "../../serialization/serialization.hpp"
#include "../../serialization/types/optional.hpp"
#include "../../util/options.hpp"
#include "../../build_info.hpp"

#if !defined(ENABLE_FZ_SERIALIZATION_DEBUG) || !ENABLE_FZ_SERIALIZATION_DEBUG
#   define PUGIXML_NO_STL
#endif

#define PUGIXML_NO_EXCEPTIONS
#define PUGIXML_NO_XPATH

#if defined(HAVE_LIBPUGIXML) && HAVE_LIBPUGIXML
#   include <pugixml.hpp>
#else
#   define PUGIXML_HEADER_ONLY
#   include "../../serialization/external/pugixml/pugixml.cpp"
#endif

namespace fz::serialization {

namespace xml_detail {
	class names_iterator {
		const char **names_{};
		std::ptrdiff_t offset_{};
		bool is_attribute_{};

		const char *tmp_[1]{};
		std::ptrdiff_t old_offset_{};

		#if ENABLE_FZ_SERIALIZATION_DEBUG
			const char **full_names_{};
		#endif
	public:
		void operator++() {
			++offset_;
		}

		void operator--() {
			--offset_;
		}

	   const char *rewind_and_substitute(const char *new_name) {
		   --offset_;

		   if (offset_ <= 0) {
				const char *old_name = names_[offset_];
				names_[offset_] = new_name;
				return old_name;
		   }

		   tmp_[0] = new_name;
		   old_offset_ = offset_;

		   names_ = tmp_;
		   offset_ = 0;

		   return nullptr;
		}

		void substitute_and_forward(const char *old_name) {
			if (old_name) {
				names_[offset_] = old_name;
			}
			else {
				offset_ = old_offset_;
			}

			++offset_;
		}

		const char *operator*() const {
			if (offset_ > 0)
				return nullptr;

			return names_[offset_];
		}

		bool is_attribute() const {
			return is_attribute_;
		}

		names_iterator(const char **names, std::size_t num_nested_names, bool is_attribute) noexcept
			: names_{names+num_nested_names}
			, offset_{-static_cast<std::ptrdiff_t>(num_nested_names)}
			, is_attribute_{is_attribute}
			#if ENABLE_FZ_SERIALIZATION_DEBUG
				, full_names_{names}
			#endif
		{}

		names_iterator() noexcept = default;
	};

}

namespace detail {

template <> struct archive_error<xml_input_archive> {
	struct element_error{
		pugi::xml_node parent;
		std::string_view name;

		std::string to_string() const;
	};

	struct attribute_error {
		pugi::xml_node parent;
		std::string_view name;

		std::string to_string() const;
	};

	struct invalid_xml {
		std::string location;
	};

	struct custom_error1 {
		std::string_view value;
	};

	struct flavour_or_version_mismatch {
		std::string description;
	};

	using suberror_type = std::variant<
		std::monostate,
		element_error,
		attribute_error,
		invalid_xml,
		custom_error1,
		flavour_or_version_mismatch,
		std::string
	>;

	struct type: archive_error_with_suberror<suberror_type> {
		using archive_error_with_suberror::archive_error_with_suberror;
		using archive_error_with_suberror::operator=;

		type(const pugi::xml_parse_result &result, native_string_view filename);

		std::string description() const;

		bool is_flavour_or_version_mismatch() const {
			return std::holds_alternative<flavour_or_version_mismatch>(suberror());
		}

		bool is_root_node_missing() const {
			if (auto e = std::get_if<element_error>(&suberror()); e && value() == ENOENT && e->parent.type() == pugi::node_document)
				return true;

			return false;
		}
	};
};

}

class xml_output_archive: public archive_output<xml_output_archive>, public archive_is_textual  {
public:
	class saver {
	public:
		virtual ~saver();

		virtual bool operator()(pugi::xml_document &document, unsigned int flags) = 0;
	};

	struct options: util::options<options, xml_output_archive> {
		opt               <std::string> root_node_name = o("filezilla");
		opt                      <bool> emit_prolog    = o(true);
		opt               <std::string> doctype        = o();
		opt                      <bool> must_indent    = o(true);
		opt <std::stack<unsigned int>*> stack          = o(nullptr);
		opt                    <bool *> save_result    = o(nullptr);
		opt                      <bool> emit_version   = o(true);

		options(){}
	};

	class buffer_saver: public saver, private pugi::xml_writer
	{
		void write(const void* data, size_t size) override;

	public:
		buffer_saver(buffer &buffer): buffer_(buffer) {}

		bool operator()(pugi::xml_document &document, unsigned int flags) override;

	private:
		buffer &buffer_;
	};

	class file_saver: public saver, private pugi::xml_writer
	{
		void write(const void* data, size_t size) override;

	public:
		file_saver(native_string filename, bool write_to_tmp_file_first = true);

		bool operator()(pugi::xml_document &document, unsigned int flags) override;

		explicit operator bool() const
		{
			return !has_error_;
		}

	private:
		native_string filename_;
		fz::file file_{};
		bool write_to_tmp_file_first_{};
		bool has_error_{};
	};

	xml_output_archive(saver &saver, options opts = {});
	~xml_output_archive();

	xml_detail::names_iterator set_next_names(xml_detail::names_iterator it)
	{
		auto prev = next_name_;

		next_name_ = std::move(it);

		return prev;
	}

	void open_node()
	{
		auto name = *next_name_;

		if (next_name_.is_attribute()) {
			if (!(cur_attribute_ = cur_node_.attribute(name)))
				cur_attribute_ = cur_node_.append_attribute(name);
		}
		else {
			if (!name) {
				char valuebuf[5+std::numeric_limits<std::decay_t<decltype(num_children_stack_.top())>>::digits10+1];
				std::sprintf(valuebuf, "value%u", num_children_stack_.top()++);
				cur_node_ = cur_node_.append_child(valuebuf);
				num_children_stack_.emplace(0);
			}
			else
			if (name[0]) {
			   cur_node_ = cur_node_.append_child(name);
			   num_children_stack_.emplace(0);
			}

			++next_name_;
		}
	}

	void close_node()
	{
		if (cur_attribute_) {
			cur_attribute_ = pugi::xml_attribute();
		}
		else {
			--next_name_;
			auto name = *next_name_;

			if (!name || name[0]) {
				num_children_stack_.pop();
				cur_node_ = cur_node_.parent();
			}
		}
	}

	auto &next_name() {
		return next_name_;
	}

private:
	saver &saver_;
	options opts_;
	pugi::xml_document document_;
	pugi::xml_node cur_node_ = document_;
	pugi::xml_attribute cur_attribute_{};

	xml_detail::names_iterator next_name_{};

	std::stack<unsigned int> &num_children_stack_;
	bool num_children_stack_allocated_{};
	const char *root_node_name_{};

public:
	template <typename T, std::enable_if_t<std::is_arithmetic_v<std::decay_t<T>>>* = nullptr>
	void set_value(T value) {
		if (cur_attribute_)
			cur_attribute_.set_value(value);
		else
			cur_node_.text().set(value);
	}

	template <typename ...Ts>
	void set_value(const std::basic_string<char, Ts...> &value) {
		if (cur_attribute_)
			cur_attribute_.set_value(value.c_str());
		else
			cur_node_.append_child(pugi::node_pcdata).set_value(value.c_str());
	}

	template <typename ...Ts>
	void set_value(const std::basic_string<wchar_t, Ts...> &value) {
		set_value(fz::to_utf8(value));
	}

	template <typename T>
	void set_value(const value_info<T> &vi) {
		pugi::xml_node info_node;

		if (vi.info) {
			info_node = cur_node_.last_child();

			if (info_node)
				info_node = cur_node_.insert_child_after(pugi::node_comment, info_node);
			else
				info_node = cur_node_.append_child(pugi::node_comment);
		}

		(*this)(vi.value);

		if (vi.info)
			info_node.set_value(vi.info);
	}
};


class xml_input_archive: public archive_input<xml_input_archive>, public archive_is_textual {
public:
	class loader {
	public:
		virtual ~loader() = default;
		virtual native_string_view name() const = 0;

		virtual error_t operator()(pugi::xml_document &document) = 0;
	};

	class buffer_loader: public loader
	{
	public:
		buffer_loader(buffer &buffer, bool parse_in_place);

		native_string_view name() const override;
		error_t operator()(pugi::xml_document &document) override;

	private:
		buffer &buffer_;
		bool parse_in_place_;
	};

	class file_loader: public loader
	{
	public:
		file_loader(native_string filename);

		native_string_view name() const override;
		error_t operator()(pugi::xml_document &document) override;

	private:
		native_string filename_;
	};

	struct options: util::options<options, xml_input_archive> {
		enum class verify_mode
		{
			ignore,
			error,
			backup
		};

		friend bool convert(std::string_view s, verify_mode &v);

		opt <std::string>     root_node_name       = o("filezilla");
		opt <verify_mode>     verify_version       = o(verify_mode::ignore);
		opt <native_string *> backup_made          = o(nullptr);
		opt <bool *>          verify_error_ignored = o(nullptr);

		options(){}
	};

	xml_input_archive(loader &loader, options opts = {});

	~xml_input_archive();

	xml_detail::names_iterator set_next_names(xml_detail::names_iterator it)
	{
		auto prev = next_name_;

		next_name_ = std::move(it);

		return prev;
	}

	void open_node()
	{
		auto name = *next_name_;

		if (next_name_.is_attribute()) {
			if (!(cur_attribute_ = cur_attribute_.next_attribute()) || std::strcmp(name, cur_attribute_.name()))
				cur_attribute_ = cur_node_.attribute(name);

			if (!cur_attribute_)
				return error({ENOENT, errors::attribute_error{cur_node_, name}});
		}
		else {
			cur_attribute_ = pugi::xml_attribute();

			// If we expect a name for the node, look for it. The algorithm used is the same as cereal's:
			//   - Try a match with the current child first.
			//   - If they don't match, restart from the first child
			//   - subsequent searches will start from the one next to the last one that matched.
			if (name && name[0]) {
				if (strcmp(name, cur_child_.name())) {
					auto next_child = cur_node_.child(name);
					if (!next_child)
						return error({ENOENT, errors::element_error{cur_node_, name}});
					cur_child_ = next_child;
				}
			 }

			 if (!name || name[0]) {
				 cur_node_ = cur_child_;
				 cur_child_ = first_element_child(cur_node_);
			 }
		}

		++next_name_;
	}

	void close_node()
	{
		if (!next_name_.is_attribute()) {
			--next_name_;
			auto name = *next_name_;

			if (!name || name[0]) {
				cur_child_ = cur_node_.next_sibling();
				auto to_delete = cur_node_;
				cur_node_ = cur_node_.parent();

				// Delete previous node so to avoid re-finding it in case of out-of-order search.
				// (But only do it if no error has been detected, in which case we need to preserve the node for error reporting,
				//  and it causes no harm because processing must (and will) stop anyway).
				// We could do
				//     to_delete.parent().remove_child(to_delete);
				// But the 'trashcan' approach is slightly faster, as it avoids deallocations.
				if (!error_)
					trashcan_.append_move(to_delete);
			}
		}
	}

	auto &next_name() {
		return next_name_;
	}

	pugi::xml_node root_node();

	template <typename... Attrs, std::enable_if_t<(std::is_constructible_v<const char *, Attrs> && ...)>* = nullptr>
	static pugi::xml_node first_element_child(pugi::xml_node n, Attrs &&... attrs) {
		for (n = n.first_child(); n && (n.type() != pugi::node_element || !(n.attribute(attrs) && ...)); n = n.next_sibling());
		return n;
	}

	build_info::version_info get_product_version() const;

private:
	using errors = detail::archive_error<xml_input_archive>;
	std::string version_check(std::string_view flavour, std::string_view version, native_string_view docname, native_string &backup_dir_name);

	options opts_;
	const char *root_node_name_{};

	pugi::xml_document document_;
	pugi::xml_node cur_node_{};
	pugi::xml_node cur_child_{};
	pugi::xml_node trashcan_{};

	pugi::xml_attribute cur_attribute_{};

	xml_detail::names_iterator next_name_{};

	build_info::version_info product_version_;

public:
	template <typename T, std::enable_if_t<std::is_arithmetic_v<T>>* = nullptr>
	void get_value(T &value)
	{
		auto get = [](const auto &src) -> decltype (auto) {
			if constexpr(std::is_floating_point_v<T>)
				return src.as_double();
			else
			if constexpr(std::is_signed_v<T>)
			   return src.as_llong();
			else
			if constexpr(std::is_same_v<bool, T>)
				return src.as_bool(true);
			else
				return src.as_ullong();
		};

		if (cur_attribute_)
			value = static_cast<T>(get(cur_attribute_));
		else
			value = static_cast<T>(get(cur_node_.text()));
	}

	template <typename ...Ts>
	void get_value(std::basic_string<char, Ts...> &value)
	{
		if (cur_attribute_)
			value = cur_attribute_.value();
		else
			value = cur_node_.child_value();
	}

	template <typename ...Ts>
	void get_value(std::basic_string<wchar_t, Ts...> &value)
	{
		if (cur_attribute_)
			value = fz::to_wstring_from_utf8(cur_attribute_.value());
		else
			value = fz::to_wstring_from_utf8(cur_node_.child_value());
	}

	using archive_input<xml_input_archive>::error;
	error_t error(int err) const {
		if (next_name_.is_attribute())
			return error({err, errors::attribute_error{cur_node_, cur_attribute_.name()}});
		else
			return error({err, errors::element_error{cur_node_.parent(), cur_node_.name()}});
	}

	void error(int err) {
		error(const_cast<const xml_input_archive *>(this)->error(err));
	}

};

/*******************/

template <typename Archive, typename T, trait::enable_if<!trait::has_minimal_serialization_v<Archive, T> &&
														 !trait::is_nvp_v<T> &&
														 !trait::is_attribute_v<T> &&
														 !trait::is_value_info_v<T> &&
														 !trait::is_optional_nvp_v<T> &&
														 !trait::is_optional_attribute_v<T>> = trait::sfinae>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
prologue(Archive &ar, const T &)
{
	ar.open_node();
}

template <typename Archive, typename T, trait::enable_if<!trait::has_minimal_serialization_v<Archive, T> &&
														 !trait::is_nvp_v<T> &&
														 !trait::is_attribute_v<T> &&
														 !trait::is_value_info_v<T> &&
														 !trait::is_optional_nvp_v<T> &&
														 !trait::is_optional_attribute_v<T>> = trait::sfinae>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
epilogue(Archive &ar, const T &)
{
	ar.close_node();
}

template <typename Archive, typename T, std::size_t N>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
serialize(Archive &ar, nvp<T,N> &v)
{
	// In textual, output archives (ie. xml, json and the like) we do not emit the optional item at all, if it's disengaged.
	if constexpr (trait::is_output_v<Archive> && util::is_optional_v<std::decay_t<T>>) {
		if (!v.value)
			return;
	}

	auto prev = ar.set_next_names({v.names.data(), N, false});
	ar(v.value);

	// In textual, input archives (ie. xml, json and the like) we allow the total absence of std::optional items.
	if constexpr (trait::is_input_v<Archive> && util::is_optional_v<std::decay_t<T>>) {
		if (ar.error() == ENOENT && ar.error_nesting_level() == ar.nesting_level()+1) {
			v.value = std::nullopt;
			ar.error(0);
		}
	}

	ar.set_next_names(std::move(prev));
}

template <typename Archive, typename T, std::size_t N>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
serialize(Archive &ar, optional_nvp<T,N> &v)
{
	auto prev = ar.set_next_names({v.names.data(), N, false});
	ar(v.value);

	// In textual, input archives (ie. xml, json and the like) we allow the total absence of items embedded in optional_nvp
	if constexpr (trait::is_input_v<Archive>) {
		if (ar.error() == ENOENT && ar.error_nesting_level() == ar.nesting_level()+1)
			ar.error(0);
	}

	ar.set_next_names(std::move(prev));
}

template <typename Archive, typename T>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
serialize(Archive &ar, attribute<T> &v)
{
	static_assert(trait::is_minimal_nvp_v<Archive, std::decay_t<nvp<T>>>, "Type T must be minimally serializable");

	auto prev = ar.set_next_names({v.names.data(), 0, true});
	ar(v.value);
	ar.set_next_names(std::move(prev));
}

template <typename Archive, typename T>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
serialize(Archive &ar, optional_attribute<T> &v)
{
	static_assert(trait::is_minimal_optional_nvp_v<Archive, std::decay_t<optional_nvp<T>>>, "Type T must be minimally serializable");

	auto prev = ar.set_next_names({v.names.data(), 0, true});
	ar(v.value);

	// In textual, input archives (ie. xml, json and the like) we allow the total absence of items embedded in optional_attribute
	if constexpr (trait::is_input_v<Archive>) {
		if (ar.error() == ENOENT && ar.error_nesting_level() == ar.nesting_level()+1)
			ar.error(0);
	}

	ar.set_next_names(std::move(prev));
}

template <typename T>
auto save(xml_output_archive &ar, const T &v) -> decltype(ar.set_value(v))
{
	ar.set_value(v);
}

template <typename T>
auto load(xml_input_archive &ar, T &v) -> decltype(ar.get_value(v))
{
	ar.get_value(v);
}

//*************************//

template <typename T>
void save(xml_output_archive &ar, const std::optional<T> &o)
{
	if (!o)
		ar.attributes(nvp(true, "xsi:nil"));
	else {
		auto old = ar.next_name().rewind_and_substitute("");
		ar(*o);
		ar.next_name().substitute_and_forward(old);
	}
}

template <typename T>
void load(xml_input_archive &ar, std::optional<T> &o)
{
	bool is_nil = false;

	if (int error = ar.attributes(nvp(is_nil, "xsi:nil")).error(); !error || error == ENOENT) {
		ar.error(0);

		if (is_nil)
			o = std::nullopt;
		else {
			auto old = ar.next_name().rewind_and_substitute("");

			if (o)
				ar(o.value());
			else
			if (T value{}; ar(value))
				o = std::move(value);

			ar.next_name().substitute_and_forward(old);
		}
	}
}

template <typename Archive, typename Key, typename Value>
trait::restrict_t<Archive, xml_input_archive, xml_output_archive>
serialize(Archive &ar, map_item<Key, Value> &item)
{
	ar.attributes(nvp(item.key, item.key_name));

	auto old = ar.next_name().rewind_and_substitute("");
	ar(item.value);
	ar.next_name().substitute_and_forward(old);
}

template <typename T>
void load(xml_input_archive &ar, const value_info<T> &vi)
{
	ar(vi.value);
}

template <typename T, std::enable_if_t<sizeof(T) == sizeof(char)>* = nullptr>
std::string save_minimal(const xml_output_archive &, const binary_data<T> &bd) {
	auto view = std::string_view(reinterpret_cast<const char *>(bd.data), bd.size);
	return fz::base64_encode(view, fz::base64_type::standard, false);
}

template <typename T, std::enable_if_t<sizeof(T) == sizeof(char)>* = nullptr>
void load_minimal(const xml_input_archive &ar, binary_data<T> &bd, const std::string &base64, xml_input_archive::error_t &error) {
	const auto &raw = fz::base64_decode(base64);
	if (raw.size() != bd.size)
		error = ar.error(EINVAL);
	else
		std::copy(raw.begin(), raw.end(), bd.data);
}

}

FZ_SERIALIZATION_LINK_ARCHIVES(xml_input_archive, xml_output_archive)

#endif // FZ_SERIALIZATION_ARCHIVES_XML_HPP
