#ifndef FZ_HTTP_MESSAGE_CONSUMER_HPP
#define FZ_HTTP_MESSAGE_CONSUMER_HPP

#include <libfilezilla/logger.hpp>

#include "../buffer_operator/line_consumer.hpp"

#include "field.hpp"

namespace fz::http
{

struct message_consumer: buffer_operator::line_consumer<buffer_line_eol::cr_lf>
{
	message_consumer(logger_interface &logger, std::size_t max_line_size);
	void reset();

protected:
	virtual int process_message_start_line(std::string_view line);
	virtual int process_message_header(field::name_view name, field::value_view value);
	virtual int process_body_chunk(buffer_string_view chunk);
	virtual int process_end_of_message_headers();
	virtual int process_end_of_message();
	virtual int process_error(int err, std::string_view msg);

	int consume_buffer() override;

	void expect_no_body();
	void set_body_consumer(buffer_operator::consumer_interface &body_consumer);

private:
	int consume_body();
	int process_buffer_line(buffer_string_view line, bool there_is_more_data_to_come) override final;

	logger_interface &logger_;

	enum status {
		parse_start_line,
		parse_headers,
		parse_trailer,
		parse_chunk_size,
		parse_end_of_chunk,
		parse_body,
		finish_consuming_body,
	} status_{};

	enum transfer_encoding {
		not_provided,
		identity,
		chunked
	} transfer_encoding_{};

	bool has_content_length_{};
	std::size_t remaining_chunk_size_ = std::numeric_limits<std::size_t>::max();
	buffer_operator::consumer_interface *body_consumer_{};
	buffer_operator::unsafe_locking_buffer body_buffer_{};
};

}

#endif // FZ_HTTP_MESSAGE_CONSUMER_HPP
