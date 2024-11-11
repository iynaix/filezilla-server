#ifndef FZ_SERIALIZATION_ARCHIVES_FWD_HPP
#define FZ_SERIALIZATION_ARCHIVES_FWD_HPP

namespace fz::serialization
{

	template <typename Derived>
	class archive_output;

	template <typename Derived>
	class archive_input;

	class archive_is_textual;
	class archive_is_input;
	class archive_is_output;

	class binary_output_archive;
	class binary_input_archive;

	class xml_output_archive;
	class xml_input_archive;

	class argv_ouput_archive;
	class argv_input_archive;

}

#endif // FZ_SERIALIZATION_ARCHIVES_FWD_HPP
