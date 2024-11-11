#ifndef COMMANDER_HPP
#define COMMANDER_HPP

#include <unordered_map>

#include <libfilezilla/string.hpp>
#include <libfilezilla/logger.hpp>

#include "../buffer_operator/file_reader.hpp"
#include "../buffer_operator/file_writer.hpp"
#include "../buffer_operator/streamed_adder.hpp"
#include "../buffer_operator/line_consumer.hpp"
#include "../buffer_operator/tvfs_entries_lister.hpp"

#include "../channel.hpp"
#include "../tvfs/engine.hpp"
#include "../tcp/session.hpp"
#include "../util/welcome_message.hpp"

#include "controller.hpp"

namespace fz::ftp {

class commander final
	: public util::invoker_handler
	, private buffer_operator::streamed_adder
	, private buffer_operator::line_consumer<buffer_line_eol::cr_lf>
	, private controller::authenticate_user_response_handler
	, private controller::make_secure_response_handler
	, private controller::data_transfer_handler
	, private controller::data_local_info_handler
	, private channel::progress_notifier
	, public enabled_for_receiving<commander>
{
	using notifier = tcp::session::notifier;

public:
	using welcome_message = util::welcome_message;

	struct buffer_operators
	{
		tvfs::entry_facts::which enabled_facts_ = tvfs::entry_facts::all;
		std::string names_prefix_;
		tvfs::entries_iterator entries_iterator_;
		tvfs::file_holder file_;

		static constexpr inline std::string_view eol = "\r\n";
		static constexpr inline std::string_view space  = " ";

		template <typename EntryStreamer, typename... Args>
		using entries_lister = buffer_operator::tvfs_entries_lister<buffer_operator::with_suffix<EntryStreamer>, std::string_view, Args...>;

		entries_lister<tvfs::entry_facts, tvfs::entry_facts::which&> facts_lister_;
		entries_lister<tvfs::entry_stats> stats_lister_;
		entries_lister<buffer_operator::with_prefix<tvfs::entry_stats>, std::string_view> stats_lister_with_prefix_space_;
		entries_lister<buffer_operator::with_prefix<tvfs::entry_name>, std::string&> names_lister_;
		entries_lister<tvfs::entry_facts, tvfs::entry_facts::which> mfmt_lister_;

		buffer_operator::file_reader file_reader_;
		buffer_operator::file_writer file_writer_;

		buffer_operators(event_loop &loop, logger_interface &logger)
			: facts_lister_(loop, entries_iterator_, eol, enabled_facts_)
			, stats_lister_(loop, entries_iterator_, eol)
			, stats_lister_with_prefix_space_(loop, entries_iterator_, eol, space)
			, names_lister_(loop, entries_iterator_, eol, names_prefix_)
			, mfmt_lister_(loop, entries_iterator_, eol, tvfs::entry_facts::which::modify)
			, file_reader_(*file_, 128*1024, &logger)
			, file_writer_(*file_, &logger)
		{}
	};

	commander(event_loop &loop, controller &co, tvfs::engine &tvfs, notifier &notifier,
			  fz::monotonic_clock &last_activity,
			  bool needs_security_before_user_cmd,
			  const welcome_message &welcome_message, const std::string &refuse_message,
			  buffer_operators &buffer_operators,
			  logger_interface &logger);
	~commander() override;

	void set_socket(socket_interface *);
	void set_timeouts(const fz::duration &login_timeout, const fz::duration &activity_timeout);
	void shutdown(int err = 0);

	bool has_empty_buffers();
	bool is_executing_command() const;

private:
	channel channel_;
	controller &controller_;
	tvfs::engine &tvfs_;
	notifier &notifier_;
	const welcome_message &welcome_message_;
	const std::string &refuse_message_;
	logger_interface &logger_;

	enum command_flags {
		none                  = 0,
		needs_arg             = 1 << 0,
		needs_auth            = 1 << 1,
		must_be_last_in_queue = 1 << 2,
		needs_security        = 1 << 3,
		trim_arg              = 1 << 4,
		needs_data_connection = 1 << 5,
	};

	enum command_reply {
		positive_preliminary_reply          = 1,
		positive_completion_reply           = 2,
		positive_intermediary_reply         = 3,
		transient_negative_completion_reply = 4,
		permanent_negative_completion_reply = 5
	};

	static bool is_negative(command_reply cr)
	{
		return cr >= transient_negative_completion_reply;
	}

	struct command {
		void (commander::*func)(std::string_view arg);
		command_flags flags;
	};

	static std::unordered_map<std::string_view, command> commands_;

	std::string upcase_str_{};
	std::string_view make_upcase(std::string_view str);

	bool is_cmd_illegal(std::string_view cmd);
	int queue_new_cmd();
	bool a_cmd_has_been_queued_{};

	#define FTP_CMD(name) \
		void name(std::string_view arg = std::string_view{}); \
		const static decltype(commands_)::const_iterator name##_cmd_

	#define FTP_CMD_ALIAS(alias, name) \
		const static decltype(commands_)::const_iterator alias##_cmd_

	FTP_CMD(ABOR);
	FTP_CMD(ADAT);
	FTP_CMD(ALLO);
	FTP_CMD(APPE);
	FTP_CMD(AUTH);
	FTP_CMD(CDUP);
	FTP_CMD(CLNT);
	FTP_CMD(CWD);
	FTP_CMD(DELE);
	FTP_CMD(EPRT);
	FTP_CMD(EPSV);
	FTP_CMD(FEAT);
	FTP_CMD(HELP);
	FTP_CMD(LIST);
	FTP_CMD(MDTM);
	FTP_CMD(MFMT);
	FTP_CMD(MKD);
	FTP_CMD(MLSD);
	FTP_CMD(MLST);
	FTP_CMD(MODE);
	FTP_CMD(NLST);
	FTP_CMD(NOOP);
	FTP_CMD(OPTS);
	FTP_CMD(PASS);
	FTP_CMD(PASV);
	FTP_CMD(PBSZ);
	FTP_CMD(PORT);
	FTP_CMD(PROT);
	FTP_CMD(PWD);
	FTP_CMD(QUIT);
	FTP_CMD(REST);
	FTP_CMD(RETR);
	FTP_CMD(RMD);
	FTP_CMD(RNFR);
	FTP_CMD(RNTO);
	FTP_CMD(SIZE);
	FTP_CMD(STAT);
	FTP_CMD(STOR);
	FTP_CMD(STRU);
	FTP_CMD(SYST);
	FTP_CMD(TYPE);
	FTP_CMD(USER);

	FTP_CMD_ALIAS(NOP, NOOP);
	FTP_CMD_ALIAS(XCWD, CWD);
	FTP_CMD_ALIAS(XRMD, RMD);
	FTP_CMD_ALIAS(XMKD, MKD);
	FTP_CMD_ALIAS(XPWD, PWD);

	#undef FTP_CMD_ALIAS
	#undef FTP_CMD

	decltype(commands_)::const_iterator current_cmd_ { commands_.cend() };
	decltype(commands_)::const_iterator cmd_being_aborted_ { commands_.cend() };

	class responder;
	template <unsigned int XYZ>
	responder respond(bool emit_xyz_on_each_line = false);

	int failure_count_{};
	void act_upon_command_reply(command_reply reply);
	void data_connection_not_setup();

	std::string user_;
	std::unique_ptr<authentication::authenticator::operation> auth_op_;

	bool only_allow_epsv_{};
	tvfs::entry_size rest_size_{};

	buffer_operators &buffer_operators_;

	std::string rename_from_{};

	bool data_is_binary_{};

	duration login_timeout_{};
	duration elapsed_login_time_{};
	duration activity_timeout_{};
	timer_id timer_id_{};
	monotonic_clock start_time_{};
	monotonic_clock &last_activity_;
	bool needs_security_before_user_cmd_{};

	// authenticate_user_response_handler interface
private:
	authentication::session_user handle_authenticate_user_response(std::unique_ptr<authentication::authenticator::operation> &&op) override;

	// make_secure_response_handler interface
private:
	void handle_make_secure_response(bool can_secure) override;

	// line_consumer interface
public:
	int process_buffer_line(buffer_string_view line, bool there_is_more_data_to_come) override;

	// event_handler interface
private:
	void operator ()(const event_base &) override;
	void on_channel_done_event(channel &, channel::error_type error);
	void on_timer_event(timer_id id);

	// data_transfer_handler interface
public:
	void handle_data_transfer(status st, channel::error_type error, std::string_view msg) override;

	// data_local_info_handler interface
public:
	void handle_data_local_info(const std::optional<std::pair<std::string, uint16_t>> &info) override;

private:
	void notify_channel_socket_read_amount(const monotonic_clock &time_point, std::int64_t) override;
	void notify_channel_socket_written_amount(const monotonic_clock &time_point, std::int64_t) override;

private:
	class async_abortable_receive: async_receive
	{
		enum state {
			idle,
			pending,
			pending_abort
		};

		state state_ = idle;
		commander &owner_;

	public:
		async_abortable_receive(commander &owner);

		template <typename F>
		auto operator >>(F && f);

		void abort();
		bool is_pending() const;
	};

	async_abortable_receive async_receive_{*this};
};

}

#endif // COMMANDS_HPP
