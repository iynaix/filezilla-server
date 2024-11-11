#ifndef FTP_TEST_CREATOR_HPP
#define FTP_TEST_CREATOR_HPP

#include "administrator.hpp"

class administrator::ftp_test_creator: public fz::event_handler
{
public:
	ftp_test_creator(administrator &admin);
	~ftp_test_creator() override;

	std::pair<std::string /*username*/, std::string /*password*/> create_environment(fz::ftp::server::options ftp_opts, fz::duration timeout);
	bool destroy_environment();

private:
	void operator()(const fz::event_base &ev) override;
	void do_destroy_environment();

	administrator &admin_;
	fz::logger::modularized logger_;

	fz::timer_id timer_id_{};
	fz::ftp::server::options previous_ftp_opts_;
	std::string temp_username_;
};

#endif // FTP_TEST_CREATOR_HPP
