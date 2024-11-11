#include <libfilezilla/socket.hpp>

#include "message_consumer.hpp"
#include "../util/parser.hpp"

#include "headers.hpp"

namespace fz::http
{

message_consumer::message_consumer(logger_interface &logger, std::size_t max_line_size)
	: line_consumer(max_line_size)
	, logger_(logger)
{
}

void message_consumer::reset()
{
	status_ = parse_start_line;
	transfer_encoding_ = not_provided;
	has_content_length_ = {};
	remaining_chunk_size_ = std::numeric_limits<std::size_t>::max();
	body_consumer_ = {};
	body_buffer_.lock()->clear();
}

int message_consumer::process_message_start_line(std::string_view)
{
	return 0;
}

int message_consumer::process_message_header(field::name_view, field::value_view)
{
	return 0;
}

int message_consumer::process_body_chunk(buffer_string_view)
{
	return 0;
}

int message_consumer::process_end_of_message_headers()
{
	return 0;
}

int message_consumer::process_end_of_message()
{
	return 0;
}

int message_consumer::process_error(int err, std::string_view msg)
{
	logger_.log_u(logmsg::error, L"%s - %s", fz::socket_error_string(err), msg);
	return err;
}

int message_consumer::consume_body()
{
	int err = body_consumer_->consume_buffer();

	if (err) {
		if (err == ECANCELED) {
			body_consumer_ = {};
			err = 0;
		}
		else
		if (err != EAGAIN && ENODATA) {
			err = process_error(err, "Error while executing the body consumer");
		}
	}

	return err;
}

int message_consumer::consume_buffer()
{
	if (status_ < parse_body) {
		return line_consumer::consume_buffer();
	}

	if (status_ == finish_consuming_body) {
		int err = consume_body();

		if (err) {
			return err;
		}

		if (!body_consumer_ || body_buffer_.lock()->empty()) {
			status_ = parse_trailer;
		}

		return 0;
	}

	auto buffer = line_consumer::get_buffer();
	auto to_consume = std::min(remaining_chunk_size_, buffer->size());

	int err = [&] {
		if (body_consumer_) {
			body_buffer_.lock()->append(buffer->get(), to_consume);
			return consume_body();
		}
		else {
			return process_body_chunk({buffer->get(), to_consume});
		}
	}();

	remaining_chunk_size_ -= to_consume;
	buffer->consume(to_consume);

	if (!err) {
		if (remaining_chunk_size_ == 0) {
			if (transfer_encoding_ == chunked)
				status_ = parse_end_of_chunk;
			else {
				reset();
				return process_end_of_message();
			}
		}
	}

	return err;
}

void message_consumer::expect_no_body()
{
	remaining_chunk_size_ = 0;
}

void message_consumer::set_body_consumer(consumer_interface &body_consumer)
{
	body_consumer_ = &body_consumer;

	body_buffer_.lock()->clear();
	body_consumer.set_buffer(&body_buffer_);
	body_consumer.set_event_handler(get_event_handler().get());
}

int message_consumer::process_buffer_line(buffer_string_view bline, bool)
{
	auto line = std::string_view((const char*)bline.data(), bline.size());

	if (status_ != parse_headers && status_ != parse_trailer) {
		logger_.log_u(logmsg::debug_debug, L"[Status: %d] %s", int(status_), line);
	}

	if (status_ == parse_start_line) {
		status_ = parse_headers;
		return process_message_start_line(line);
	}

	if (status_ == parse_headers || status_ == parse_trailer) {
		if (line.empty()) {
			if (status_ == parse_headers) {
				if (int err = process_end_of_message_headers())
					return err;

				if (transfer_encoding_ == chunked) {
					if (has_content_length_) {
						return process_error(EINVAL, "Content-Length and chunked Transfer-Encoding are not compatible");
					}

					status_ = parse_chunk_size;
					return 0;
				}

				if (remaining_chunk_size_ != 0) {
					status_ = parse_body;
					return 0;
				}

				// Else fall through and end the message.
			}

			reset();
			return process_end_of_message();
		}

		util::parseable_range r(line);
		std::string_view name, value;

		static constexpr std::string_view token_chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789"
			"_-";

		if (!(parse_until_not_lit(r, name, token_chars) && match_string(r, ": ") && parse_until_eol(r, value))) {
			return process_error(EINVAL, fz::sprintf("Invalid header line: %s", line));
		}

		if (logger_.should_log(logmsg::debug_debug)) {
			std::string_view log_value
				= name == headers::Cookie || name == headers::Authorization ?
					  "<redacted for privacy>"
				: value;

			logger_.log(logmsg::debug_debug, L"[Status: %d] %s: %s", int(status_), name, log_value);
		}

		if (fz::starts_with<true>(std::string_view(name), std::string_view("X-FZ-INT-"))) {
			// Do not process header field names starting with X-FZ-INT-, since they are used internally
			// by our framework and no client should be sending them
			return process_error(EINVAL, fz::sprintf("Client sent a X-FZ-* header: %s.", line));
		}

		if (int err = process_message_header(name, value)) {
			return err;
		}

		if (name == headers::Transfer_Encoding) {
			if (field::value_view(value).as_list().last().is("identity"))
				transfer_encoding_ = identity;
			else
			if (field::value_view(value).as_list().last().is("chunked"))
				transfer_encoding_ = chunked;
			else
				return process_error(EINVAL, fz::sprintf("Unsupported Transfer-Encoding: %s", value));
		}
		else
		if (name == headers::Content_Length) {
			util::parseable_range r(value);

			if (!(parse_int(r, remaining_chunk_size_) && eol(r))) {
				return process_error(EINVAL, fz::sprintf("Invalid Content-Length: %s", value));
			}

			has_content_length_ = true;
		}

		return 0;
	}

	if (status_ == parse_chunk_size) {
		util::parseable_range r(line);

		// FIXME: We ignore extensions for now.
		if (!(parse_int(r, remaining_chunk_size_, 16) && (lit(r, ';') || eol(r)))) {
			return process_error(EINVAL, fz::sprintf("Invalid chunk size: %s", line));
		}

		if (remaining_chunk_size_ > 0) {
			status_ = parse_body;
		}
		else
		if (body_buffer_.lock()->empty()) {
			status_ = parse_trailer;
		}
		else {
			status_ = finish_consuming_body;
		}

		return 0;
	}

	if (status_ == parse_end_of_chunk) {
		if (!line.empty())
			return process_error(EINVAL, fz::sprintf("Spurious data after end of chunk: %s", line));

		status_ = parse_chunk_size;

		return 0;
	}

	return process_error(EINVAL, fz::sprintf("Invalid internal status: %d.", status_));
}

}
