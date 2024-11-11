#ifndef ADMINISTRATOR_NOTIFIER_HPP
#define ADMINISTRATOR_NOTIFIER_HPP

#include "administrator.hpp"

class administrator::log_forwarder: public fz::logger::modularized {
public:
	log_forwarder(administrator &administrator, fz::tcp::session::id session_id);
	log_forwarder(logger_interface &parent, administrator &administrator, fz::tcp::session::id session_id);

	void do_log(fz::logmsg::type t, const info_list &l, std::wstring &&message) override;

	void detach_from_administrator();

private:
	administrator *administrator_;
	fz::tcp::session::id session_id_;

	mutable fz::mutex mutex_;
};

class administrator::notifier: public fz::tcp::session::notifier {
public:
	~notifier() override;

	void notify_user_name(std::string_view name) override;

	void notify_entry_open(std::uint64_t id, std::string_view path, std::int64_t size) override;
	void notify_entry_close(std::uint64_t id, int error) override;
	void notify_entry_write(std::uint64_t id, std::int64_t amount, std::int64_t offset) override;
	void notify_entry_read(std::uint64_t id, std::int64_t amount, std::int64_t offset) override;

	void notify_protocol_info(const protocol_info &info) override;

	fz::logger_interface &logger() override;

	notifier(administrator &administrator, fz::tcp::session::id id, const fz::datetime &start, const std::string &peer_ip, fz::address_type peer_address_type, fz::logger_interface &logger);

	void send_session_info(administration::engine::session &session) const;
	void detach_from_administrator();

private:
	administrator *administrator_;

	std::uint64_t session_id_;
	fz::datetime start_;
	fz::monotonic_clock monotonic_start_;
	std::string peer_ip_;
	fz::address_type peer_address_type_;

	std::string user_name_;
	fz::duration user_name_set_time_{};

	struct entry {
		std::string path{};
		int64_t size{};
		int64_t written{};
		int64_t read{};
		fz::duration open_time_{};
		fz::duration last_written_time_{};
		fz::duration last_read_time_{};

		fz::duration last_reported_written_time_{};
		fz::duration last_reported_read_time_{};
	};

	std::map<std::uint64_t, entry> entries_;

	std::optional<administration::session::any_protocol_info> proto_info_;
	fz::duration proto_info_set_time_;

	log_forwarder log_forwarder_;

	mutable fz::mutex mutex_;
};

#endif // ADMINISTRATOR_NOTIFIER_HPP
