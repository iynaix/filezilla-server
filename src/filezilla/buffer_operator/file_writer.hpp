#ifndef FZ_BUFFER_OPERATOR_FILE_WRITER_HPP
#define FZ_BUFFER_OPERATOR_FILE_WRITER_HPP

#include <libfilezilla/file.hpp>
#include <libfilezilla/logger.hpp>

#include "../strresult.hpp"
#include "../strsyserror.hpp"

#include "../buffer_operator/consumer.hpp"

namespace fz::buffer_operator {

	class file_writer: public consumer {
	public:
		explicit file_writer(file &file, logger_interface *logger = nullptr)
			: file_{file}
			, logger_{logger}
		{}

		int consume_buffer() override {
			auto buffer = get_buffer();
			if (!buffer)
				return EFAULT;

			auto result = file_.write2(buffer->get(), buffer->size());
			if (result.error_) {
				if (logger_) {
					logger_->log_u(logmsg::error, L"Error while writing to file: %s.", strresult(result));
					logger_->log_u(logmsg::debug_debug, L"write2: res = %d (raw = %d: %s)", result.error_, result.raw_, strsyserror(result.raw_));
				}

				return EIO;
			}

			buffer->consume(result.value_);
			return 0;
		}

	private:
		file &file_;
		logger_interface *logger_;
	};

}

#endif // FZ_BUFFER_OPERATOR_FILE_WRITER_HPP
