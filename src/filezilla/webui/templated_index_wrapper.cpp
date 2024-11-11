#include "templated_index_wrapper.hpp"

#include "../http/server/responder.hpp"
#include "../build_info.hpp"
#include "../util/io.hpp"

namespace fz::webui {

void templated_index_wrapper::handle_transaction(const http::server::shared_transaction &t) {
	using namespace std::string_view_literals;

	if (t->req().uri.path_ != "/index.html"sv) {
		return fs_.handle_transaction(t);
	}

	struct transaction: http::server::transaction
	{
		struct responder: http::server::responder
		{
			responder(templated_index_wrapper &owner, http::server::responder &res)
				: owner_(owner)
				, res_(res)
			{}

			bool send_body(tvfs::file_holder file) override
			{
				if (!file) {
					return res_.send_body(std::move(file));
				}

				scoped_lock lock(owner_.mutex_);

				if (!owner_.mtime_ || owner_.mtime_ != file->get_modification_time()) {
					fz::buffer buf;
					if (!util::io::read(*file, buf)) {
						res_.abort_send("Failed reading from file.");
						return false;
					}

					owner_.body_.assign(buf.to_view());
					owner_.mtime_ = file->get_modification_time();

					// Do the proper templating, finally.
					fz::replace_substrings(owner_.body_, "{{PRODUCT_NAME}}", fz::build_info::package_name);
					fz::replace_substrings(owner_.body_, "{{PRODUCT_VERSION}}", fz::to_string(fz::build_info::version));
				}

				return res_.send_body(owner_.body_);
			}

			bool send_status(unsigned int code, std::string_view reason = {}) override
			{
				return res_.send_status(code, reason);
			}

			bool send_headers(std::initializer_list<std::pair<http::field::name_view, http::field::value_view>> list) override
			{
				return res_.send_headers(list);
			}

			bool send_body(std::string_view body) override
			{
				return res_.send_body(body);
			}

			bool send_body(tvfs::entries_iterator it) override
			{
				return res_.send_body(std::move(it));
			}

			bool send_end() override
			{
				return res_.send_end();
			}

			void abort_send(std::string_view msg) override
			{
				res_.abort_send(msg);
			}

		private:
			templated_index_wrapper &owner_;
			http::server::responder &res_;
		};

		transaction(templated_index_wrapper &owner, http::server::shared_transaction t)
			: t_(std::move(t))
			, responder_(owner, t_->res())
		{}

		http::server::request &req() override
		{
			return t_->req();
		}

		http::server::responder &res() override
		{
			return responder_;
		}

		class event_loop &get_event_loop() override
		{
			return t_->get_event_loop();
		}

		util::locked_proxy<http::server::session> get_session() override
		{
			return t_->get_session();
		}

	private:
		http::server::shared_transaction t_;
		responder responder_;
	};

	return fs_.handle_transaction(std::make_shared<transaction>(*this, t));
}

}
