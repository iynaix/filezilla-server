#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include "../administrator.hpp"

#include "../filezilla/update/checker.hpp"
#include "../../src/filezilla/update/raw_data_retriever/http.hpp"
#include "../../src/filezilla/update/info_retriever/chain.hpp"

class administrator::update_checker: public fz::event_handler, public fz::enabled_for_receiving<update_checker> {
public:
	using options = fz::update::checker::options;

	update_checker(administrator &admin, const fz::native_string &cachepath, options opts = {});
	~update_checker() override;

	void set_options(options opts);

	void start();
	void stop();

	bool check_now();

	void handle_response(administration::retrieve_update_raw_data::response &&v, administration::engine::session::id id = {});
	void on_disconnection(administration::engine::session::id id);

	fz::update::info get_last_checked_info() const;
	fz::datetime get_last_check_dt() const;
	fz::datetime get_next_check_dt() const;

private:
	class admin_retriever: private event_handler, public fz::update::info::raw_data_retriever
	{
	public:
		admin_retriever(update_checker &checker, fz::logger_interface &logger);
		~admin_retriever() override;

		void retrieve_raw_data(bool manual, fz::receiver_handle<result>) override;
		void handle_response(administration::retrieve_update_raw_data::response &&v, administration::engine::session::id id = {});
		void on_disconnection(administration::engine::session::id id);

	private:
		void send_request();
		void operator()(const fz::event_base &ev) override;

		update_checker &checker_;
		fz::logger::modularized logger_;
		fz::receiver_handle<result> handle_;
		bool manual_{};
		std::size_t remaining_attempts_{};
		fz::timer_id timer_id_{};
		administration::engine::session::id session_id_{};
	};

	void operator()(const fz::event_base &ev) override;

	administrator &admin_;
	fz::native_string cachepath_;

	fz::logger::modularized logger_;
	fz::update::raw_data_retriever::http http_retriever_;
	admin_retriever admin_retriever_;
	fz::update::info_retriever::chain chain_;
	fz::update::checker checker_;
};


#endif // ADMINISTRATOR_UPDATER_HPP
