#ifndef FZ_BUFFER_OPERATOR_FILE_READER_HPP
#define FZ_BUFFER_OPERATOR_FILE_READER_HPP

#include <libfilezilla/file.hpp>
#include <libfilezilla/logger.hpp>

#include "../strresult.hpp"
#include "../strsyserror.hpp"

#include "adder.hpp"

namespace fz::buffer_operator {

	class file_reader: public adder {
	public:
		file_reader(file &file, unsigned int max_buffer_size, logger_interface *logger = nullptr)
			: file_{file}
			, max_buffer_size_{max_buffer_size}
			, logger_(logger)
		{}

		int add_to_buffer() override {
			auto buffer = get_buffer();
			if (!buffer)
				return EFAULT;

			if (max_buffer_size_ <= buffer->size() )
				return ENOBUFS;

			std::size_t to_read = max_buffer_size_ - buffer->size();

			auto result = file_.read2(buffer->get(to_read), to_read);
			if (result.error_) {
				if (logger_) {
					logger_->log_u(logmsg::error, L"Error while reading from file: %s.", strresult(result));
					logger_->log_u(logmsg::debug_debug, L"read2: res = %d (raw = %d: %s)", result.error_, result.raw_, strsyserror(result.raw_));
				}

				return EIO;
			}

			if (!result.value_)
				return ENODATA;

			buffer->add(result.value_);
			return 0;
		}

	private:
		file &file_;
		unsigned int max_buffer_size_;
		logger_interface *logger_;
	};

}
#endif // FZ_BUFFER_OPERATOR_FILE_READER_HPP
