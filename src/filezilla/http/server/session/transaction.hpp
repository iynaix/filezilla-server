#ifndef FZ_HTTP_SERVER_SESSION_TRANSACTION_HPP
#define FZ_HTTP_SERVER_SESSION_TRANSACTION_HPP

#include "../transaction.hpp"
#include "../session.hpp"

namespace fz::http {

struct server::session::transaction: public server::transaction, private server::responder
{
	struct file_reader: buffer_operator::delegate_adder
	{
		file_reader(tvfs::file_holder file, logger_interface &logger)
			: delegate_adder(fr_)
			, file_(std::move(file))
			, fr_(*file_, 128*1024, &logger)
		{}

	private:
		tvfs::file_holder file_;
		buffer_operator::file_reader fr_;
	};

	struct plain_entries_reader: fz::buffer_operator::tvfs_entries_lister<fz::buffer_operator::with_suffix<tvfs::entry_stats>, std::string_view>
	{
		plain_entries_reader(event_loop &loop, tvfs::entries_iterator it)
			: tvfs_entries_lister(loop, it_, "\n")
			, it_(std::move(it))
		{}

	private:
		tvfs::entries_iterator it_;
	};

	struct html_entry_stats: tvfs::customizable_entry_stats<html_entry_stats>
	{
		using customizable_entry_stats::customizable_entry_stats;

		void stream_name_to(util::buffer_streamer &bs) const;
	};

	struct html_entries_reader: fz::buffer_operator::tvfs_entries_lister<fz::buffer_operator::with_suffix<html_entry_stats>, std::string_view>
	{
		html_entries_reader(event_loop &loop, tvfs::entries_iterator it);

		void set_buffer(buffer_operator::locking_buffer *b) override;
		int add_to_buffer() override;

	private:
		tvfs::entries_iterator it_;
		std::string prologue_;
		static const std::string_view epilogue_;
	};

	struct ndjson_entry
	{
		ndjson_entry(const tvfs::entry &e)
			: e_{e}
		{}

		void operator()(fz::util::buffer_streamer &bs) const;

	protected:
		const tvfs::entry &e_;
	};

	struct ndjson_entries_reader: fz::buffer_operator::tvfs_entries_lister<fz::buffer_operator::with_suffix<ndjson_entry>, std::string_view>
	{
		ndjson_entries_reader(event_loop &loop, tvfs::entries_iterator it)
			: tvfs_entries_lister(loop, it_, "\n")
			, it_(std::move(it))
		{}

	private:
		tvfs::entries_iterator it_;
	};

	struct no_reader: buffer_operator::no_adder
	{
		using no_adder::no_adder;
	};


	struct response {
		enum status {
			waiting_for_code_and_reason = 0,
			waiting_for_headers = 1,
			waiting_for_body = 2,
			sending_body = 3,
			sent_body = 4,
			ended = 5
		};

		status status_{};
		fz::buffer headers_buffer_;

		std::variant<no_reader, file_reader, plain_entries_reader, html_entries_reader, ndjson_entries_reader> body_reader_;
		std::optional<body_chunker> body_chunker_;

		bool chunked_encoding_is_supported_{true};
		bool chunked_encoding_requested_{false};
		bool close_connection_{};
		headers::mapped_type content_type;
	};

	struct no_writer: buffer_operator::no_consumer
	{
		using no_consumer::no_consumer;

		void on_end(bool)
		{}
	};

	struct file_writer: buffer_operator::delegate_consumer
	{
		file_writer(tvfs::file_holder file, logger_interface &logger, std::function<void(tvfs::file_holder file, bool success)> on_end)
			: delegate_consumer(fw_)
			, file_(std::move(file))
			, fw_(*file_, &logger)
			, on_end_(std::move(on_end))
		{}

		void on_end(bool success)
		{
			if (on_end_) {
				on_end_(std::move(file_), success);
			}
		}

	private:
		tvfs::file_holder file_;
		buffer_operator::file_writer fw_;
		std::function<void(tvfs::file_holder file, bool success)> on_end_;
	};

	struct string_writer: buffer_operator::consumer
	{
		string_writer(std::string s, std::function<void(std::string s, bool success)> on_end)
			: s_(std::move(s))
			, on_end_(std::move(on_end))
		{}

		int consume_buffer() override
		{
			auto buffer = get_buffer();
			if (!buffer) {
				return EFAULT;
			}

			s_.append(reinterpret_cast<char*>(buffer->data()), buffer->size());
			buffer->consume(buffer->size());

			return 0;
		}

		void on_end(bool success)
		{
			if (on_end_) {
				on_end_(std::move(s_), success);
			}
		}

	private:
		std::string s_;
		std::function<void(std::string s, bool success)> on_end_;
	};

	struct request: server::request
	{
		request(transaction &t)
			: server::request(t)
		{}

		~request() override;

		bool close_connection_{};
		bool got_end_of_message_{};
		bool waiting_for_consumer_event_{};

		std::variant<no_writer, file_writer, string_writer> body_writer_{};
	};

	request &req() override;
	responder &res() override;
	util::locked_proxy<session> get_session() override;
	event_loop &get_event_loop() override;

	transaction(event_loop &event_loop, session &s);
	~transaction() override;
	transaction(const transaction &) = delete;
	transaction(transaction &&) = delete;
	void detach();

private:
	friend session;

	event_loop &event_loop_;
	mutex mutex_;
	session *s_;

	request request_;
	response response_;

private:
	bool send_status(unsigned int code, std::string_view reason) override;
	bool send_headers(std::initializer_list<std::pair<field::name_view, field::value_view> >) override;
	bool send_body(std::string_view) override;
	bool send_body(tvfs::file_holder file) override;
	bool send_body(tvfs::entries_iterator it) override;
	bool send_end() override;
	void abort_send(std::string_view msg) override;

};

}

#endif // FZ_HTTP_SERVER_SESSION_TRANSACTION_HPP
