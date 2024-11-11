#include <unordered_set>

#include <libfilezilla/string.hpp>

#include "../hostaddress.hpp"
#include "../tvfs/engine.hpp"
#include "../util/parser.hpp"
#include "../build_info.hpp"
#include "../strresult.hpp"

#include "commander.hpp"

namespace fz::ftp {

using namespace std::string_view_literals;

std::unordered_map<std::string_view, commander::command> commander::commands_;

#define FTP_CMD(name, flags) \
	const decltype(commander::commands_)::const_iterator commander::name##_cmd_ { commander::commands_.try_emplace(#name, commander::command{&commander::name, commander::command_flags(flags)}).first }; \
	void commander::name([[maybe_unused]] std::string_view arg)

#define CUR_FTP_CMD_IS(name) (name ##_cmd_ == current_cmd_)

#define FTP_CMD_ALIAS(alias, name) \
	const decltype(commander::commands_)::const_iterator commander::alias##_cmd_ { commander::commands_.try_emplace(#alias, commander::name##_cmd_->second).first }

namespace {

	std::tuple<char, std::string, char> quote(std::string_view str)
	{
		return {'"', fz::replaced_substrings(str, "\"", "\"\""), '"'};
	}

	struct line_ender{};
	static constexpr line_ender endl{};

	constexpr unsigned int max_line_size = 4096;

}

class commander::responder
{
	static constexpr auto eol = std::array<char, 2>{'\r', '\n'};

public:
	using xyz_t = const std::array<char,3>;

	responder(commander &commander, xyz_t xyz, bool emit_xyz_on_each_line = false)
		: commander_{commander},
		  xyz_{std::move(xyz)},
		  out_{commander_.buffer_stream()},
		  sol_ref_{},
		  line_{0},
		  emit_xyz_on_each_line_(emit_xyz_on_each_line)

	{
		out_ << sol_ref_ << xyz_;
	}

	template <typename T>
	responder &operator <<(const T &v) noexcept
	{
		out_ << ' ' << v;

		return *this;
	}

	responder &operator <<(const line_ender &) noexcept
	{
		if (line_++ == 0 || emit_xyz_on_each_line_) {
			const bool has_space_after_code = out_.cur() > &sol_ref_[3];

			if (has_space_after_code)
				sol_ref_[3] = '-';
			else
				out_ << '-';

			auto reply_str = std::string_view{(const char *)&sol_ref_, std::size_t(out_.cur()-&sol_ref_)};

			commander_.logger_.log_u(logmsg::reply, L"%s", reply_str);
		}

		out_ << eol << sol_ref_;

		if (emit_xyz_on_each_line_)
			out_ << xyz_;

		return *this;
	}

	~responder()
	{
		if (line_ > 0 && !emit_xyz_on_each_line_) {
			util::buffer_streamer::ref eol_ref;
			out_ << eol_ref << xyz_;
			std::rotate(&sol_ref_, &eol_ref, &eol_ref+3);
		}

		auto reply_str = std::string_view{(const char *)&sol_ref_, std::size_t(out_.cur()-&sol_ref_)};

		commander_.logger_.log_u(logmsg::reply, L"%s", reply_str);

		out_ << eol;

		commander_.act_upon_command_reply(static_cast<command_reply>(xyz_[0]-'0'));
	}

	responder(const responder &) = delete;
	responder(responder &&) = delete;

private:
	commander &commander_;
	xyz_t xyz_;
	util::buffer_streamer out_;
	util::buffer_streamer::ref sol_ref_;
	std::size_t line_;
	bool emit_xyz_on_each_line_;
};

template <unsigned int XYZ>
commander::responder commander::respond(bool emit_xyz_on_each_line)
{
	constexpr auto x = (XYZ/100) % 10;
	constexpr auto y = (XYZ/10) % 10;
	constexpr auto z = (XYZ/1) % 10;

	static_assert(x >= 1 && x <= 5, "Only numbers between 1 and 5 can be used as the first reply code digit");
	static_assert(y >= 0 && y <= 5, "Only numbers between 0 and 5 can be used as the second reply code digit");
	static_assert(z >= 0 && z <= 9, "Only numbers between 0 and 9 can be used as the third reply code digit");

	return {*this, {x+'0', y+'0', z+'0'}, emit_xyz_on_each_line};
}

commander::async_abortable_receive::async_abortable_receive(commander &owner)
	: async_receive(owner)
	, owner_(owner)
{}

void commander::async_abortable_receive::abort()
{
	if (state_ == pending)
		state_ = pending_abort;
}

bool commander::async_abortable_receive::is_pending() const
{
	return state_ == pending;
}

template <typename F>
auto commander::async_abortable_receive::operator >>(F && f)
{
	state_ = pending;

	return async_receive::operator>>([this, f=std::forward<F>(f)](auto &&... args) {
		if (state_ == pending_abort)
			owner_.consumer::send_event(0);
		else {
			f(std::forward<decltype(args)>(args)...);

			if (state_ == pending_abort)
				owner_.consumer::send_event(0);
		}

		state_ = idle;
	});
}

commander::commander(event_loop &loop, controller &co, tvfs::engine &tvfs, notifier &notifier,
					 monotonic_clock &last_activity,
					 bool needs_security_before_user_cmd,
					 const welcome_message &welcome_message, const std::string &refuse_message,
					 buffer_operators &buffer_operators,
					 logger_interface &logger)
	: invoker_handler(loop)
	, streamed_adder()
	, line_consumer{max_line_size}
	, channel_{*this, max_line_size, 5, false, *this, 0}
	, controller_{co}
	, tvfs_{tvfs}
	, notifier_{notifier}
	, welcome_message_(welcome_message)
	, refuse_message_(refuse_message)
	, logger_{logger}
	, buffer_operators_(buffer_operators)
	, last_activity_(last_activity)
	, needs_security_before_user_cmd_(needs_security_before_user_cmd)
{
}

commander::~commander()
{
	remove_handler_and_stop_receiving();
}

void commander::set_socket(socket_interface *si)
{
	if (si == channel_.get_socket())
		return;

	channel_.set_socket(si, false);

	if (si) {
		channel_.set_buffer_adder(this);
		channel_.set_buffer_consumer(this);

		{
			auto res = respond<220>(true);

			res << build_info::package_name;

			if (welcome_message_.has_version)
				res << fz::to_string(build_info::version);

			res << endl << "Please visit" << build_info::url;

			if (!build_info::warning_message.empty()) {
				for (auto l: fz::strtokenizer(build_info::warning_message, "\r\n", false)) {
					res << endl << l;
				}
			}

			if (const auto &msg = refuse_message_.empty() ? welcome_message_ : refuse_message_; !msg.empty()) {
				for (auto l: fz::strtokenizer(msg, "\r\n", false)) {
					res << endl << l;
				}
			}
		}

		if (!refuse_message_.empty())
			shutdown(ECONNREFUSED);

		start_time_ = fz::monotonic_clock::now();
		last_activity_ = start_time_;

		set_timeouts(login_timeout_, activity_timeout_);
	}
}

void commander::set_timeouts(const duration &login_timeout, const duration &activity_timeout)
{
	login_timeout_ = login_timeout;
	activity_timeout_ = activity_timeout;

	stop_timer(timer_id_);
	timer_id_ = 0;

	if (channel_.get_socket()) {
		if (!controller_.is_authenticated()) {
			if (login_timeout) {
				timer_id_ = add_timer(start_time_ + login_timeout - fz::monotonic_clock::now() - elapsed_login_time_, true);
			}
		}
		else
		if (activity_timeout) {
			timer_id_ = add_timer(last_activity_ + activity_timeout - fz::monotonic_clock::now(), true);
		}
	}
}

void commander::on_timer_event(timer_id id)
{
	if (id != timer_id_)
		return;

	const auto timeout = [this](const char *msg) {
		if (controller_.is_securing())
			return controller_.quit(ETIMEDOUT);

		respond<421>() << msg;
		return shutdown(ETIMEDOUT);
	};

	if (!controller_.is_authenticated()) {
		controller_.stop_ongoing_user_authentication();
		auth_op_.reset(); // not valid anymore.
		return timeout("Login time exceeded. Closing control connection.");
	}

	auto delta = !last_activity_ ? activity_timeout_ : monotonic_clock::now() - last_activity_;
	if (delta >= activity_timeout_)
		return timeout("Activity timeout.");

	timer_id_ = add_timer(activity_timeout_ - delta, true);
}

void commander::shutdown(int err)
{
	// Disallow further processing of input
	channel_.set_buffer_consumer(nullptr);
	channel_.shutdown(err);
}

bool commander::has_empty_buffers()
{
	auto adder_empty = adder::get_buffer()->empty();
	auto consumer_empty = consumer::get_buffer()->empty();

	return adder_empty && consumer_empty;
}

bool commander::is_executing_command() const
{
	return current_cmd_ != commands_.cend();
}

void commander::act_upon_command_reply(command_reply reply)
{
	if (reply != positive_preliminary_reply) {
		// Command has finished execution.
		current_cmd_ = commands_.cend();

		if (a_cmd_has_been_queued_) {
			// Inform the consumer it can start dequeing commands again;
			a_cmd_has_been_queued_ = false;
			line_consumer::send_event(0);
		}

		if (reply != positive_intermediary_reply) {
			buffer_operators_.file_ = {};
			buffer_operators_.entries_iterator_.end_iteration();
			rest_size_ = 0;
		}
	}

	// Quit after 5 failed commands if not already authenticated
	if (reply == permanent_negative_completion_reply) {
		if (!controller_.is_authenticated() && ++failure_count_ == 5)
			shutdown();
	}
	else {
		failure_count_ = 0;
	}
}

void commander::data_connection_not_setup()
{
	respond<425>() << [&] {
		if (only_allow_epsv_) {
			return "Use EPSV first.";
		}
		else
		if (controller_.get_control_socket_address_family() == fz::address_type::ipv6) {
			return "Use EPRT or EPSV first.";
		}

		return "Use PORT, PASV or EPSV first.";
	}();
}

std::string_view commander::make_upcase(std::string_view str)
{
	upcase_str_.clear();
	upcase_str_.reserve(str.size());

	for (auto c: str)
		upcase_str_.push_back(fz::toupper_ascii(c));

	return upcase_str_;
}

bool commander::is_cmd_illegal(std::string_view cmd)
{
	static const std::unordered_set<std::string_view> illegal_cmds = {
		"GET", "HEAD", "POST", "PUT",
		"PATCH", "MKCOL", "PROPFIND", "MOVE", "PROPPATCH", "COPY", "LOCK", "UNLOCK",
		"EHLO", "HELO", "MAIL", "STARTTLS"
		"APOP"
	};

	if (illegal_cmds.count(cmd) != 0)
		return true;

	if (fz::starts_with(cmd, std::string_view("HTTP/")))
		return true;

	if (cmd[0] <= 0x1F)
		return true;

	if (fz::ends_with(cmd, std::string_view(":")))
		return true;

	return false;
}

int commander::queue_new_cmd()
{
	logger_.log_u(logmsg::status, L"A command is already being processed. Queuing the new one until the current one is finished.");
	a_cmd_has_been_queued_ = true;
	return EAGAIN;
}

int commander::process_buffer_line(buffer_string_view line, bool there_is_more_data_to_come)
{
	if (!channel_.get_socket()) {
		logger_.log_u(logmsg::debug_debug, L"The channel has been shut off, soon a channel::done_event will arrive. We won't process further commands.");
		return 0;
	}

	std::string_view cmd_line{reinterpret_cast<const char *>(line.data()), line.size()};

	auto cmd = cmd_line.substr(0, std::min(cmd_line.find(' '), cmd_line.size()));
	auto arg = cmd_line.substr(cmd.size());
	arg.remove_prefix(!arg.empty());

	auto log_cmd_line = [&] {
		std::string_view to_log = cmd == "PASS" ? "PASS ****" : cmd_line;

		logger_.log_u(fz::logmsg::command, L"%s", to_log);
	};

	if (!cmd.empty()) {
		cmd = make_upcase(cmd);

		log_cmd_line();

		auto new_cmd = commands_.find(cmd);
		if (new_cmd == commands_.cend()) {
			if (is_cmd_illegal(cmd)) {
				respond<501>() << "What are you trying to do? Go away.";
				shutdown();
				return 0;
			}

			if (is_executing_command())
				return queue_new_cmd();

			respond<500>() << "Wrong command.";
			return 0;
		}

		if (new_cmd != ABOR_cmd_) {
			if (is_executing_command())
				return queue_new_cmd();
		}
		else {
			if (CUR_FTP_CMD_IS(ABOR) || CUR_FTP_CMD_IS(USER) || CUR_FTP_CMD_IS(PASS))
				return queue_new_cmd();

			if (async_receive_.is_pending()) {
				async_receive_.abort();
				return EAGAIN;
			}

			cmd_being_aborted_ = current_cmd_;
		}

		current_cmd_ = new_cmd;

		if (current_cmd_->second.flags & needs_arg && arg.empty()) {
			respond<501>() << "Missing required argument";
			return 0;
		}

		if (current_cmd_->second.flags & needs_auth && !controller_.is_authenticated()) {
			respond<530>() << "Please log in with USER and PASS first.";
			return 0;
		}

		if (current_cmd_->second.flags & must_be_last_in_queue && there_is_more_data_to_come) {
			respond<503>() << "Spurious data after" << current_cmd_->first << "command.";
			shutdown();
			return 0;
		}

		if (current_cmd_->second.flags & needs_security && !controller_.is_secure()) {
			respond<502>() << "Command" << current_cmd_->first << "not implemented for this authentication type";
			return 0;
		}

		if (current_cmd_->second.flags & trim_arg) {
			arg.remove_prefix(std::min(arg.find_first_not_of(' '), arg.size()));
		}

		if (current_cmd_->second.flags & needs_data_connection) {
			if (!controller_.is_data_connection_setup()) {
				data_connection_not_setup();
				return 0;
			}
		}

		(this->*current_cmd_->second.func)(arg);
	}
	else
	if (!arg.empty()) {
		log_cmd_line();

		respond<500>() << "Syntax error, command line not recognized.";
	}

	return 0;
}

void commander::operator()(const event_base &event)
{
	if (on_invoker_event(event))
		return;

	fz::dispatch<
		channel::done_event,
		timer_event
	>(event, this,
		&commander::on_channel_done_event,
		&commander::on_timer_event
	);
}

void commander::on_channel_done_event(channel &, channel::error_type error)
{
	if (error && error != ECONNRESET) {
		auto level = logmsg::error;
		if (error == ECONNABORTED && controller_.must_downgrade_log_level())
			level = logmsg::debug_warning;

		logger_.log_u(level, "Control channel closed with error from source %d. Reason: %s.", (int)error.source(), socket_error_description(error.error()));
	}

	controller_.quit(error);
}

void commander::notify_channel_socket_read_amount(const monotonic_clock &time_point, int64_t amount)
{
	last_activity_ = time_point;
	notifier_.notify_entry_write(0, amount, -1);
}

void commander::notify_channel_socket_written_amount(const monotonic_clock &time_point, int64_t amount)
{
	last_activity_ = time_point;
	notifier_.notify_entry_read(0, amount, -1);
}

FTP_CMD(FEAT, none) {
	respond<211>()
		<< "Features:" << endl
		<< "MDTM" << endl
		<< "REST STREAM" << endl
		<< "SIZE" << endl
		<< "MLST" << tvfs::entry_facts::feat(buffer_operators_.enabled_facts_) << endl
		<< "MLSD" << endl
		<< "AUTH SSL" << endl
		<< "AUTH TLS" << endl
		<< "PROT" << endl
		<< "PBSZ" << endl
		<< "UTF8" << endl
		<< "TVFS" << endl
		<< "EPSV" << endl
		<< "EPRT" << endl
		<< "MFMT" << endl
		<< "End";
}

FTP_CMD(OPTS, needs_arg) {
	auto opts = fz::strtok_view(arg, " ");

	if (opts.size() == 2 && (equal_insensitive_ascii(opts[0], "UTF8") || equal_insensitive_ascii(arg, "UTF-8"))) {
		if (equal_insensitive_ascii(opts[1], "ON")) {
			respond<202>() << "UTF8 mode is always enabled. No need to send this command";
			return;
		}

		if (equal_insensitive_ascii(opts[1], "OFF")) {
			respond<504>() << "UTF8 mode cannot be disabled";
			return;
		}
	}
	else
	if (!opts.empty() && opts.size() <= 2 && equal_insensitive_ascii(opts[0], "MLST")) {
		buffer_operators_.enabled_facts_ = {};

		respond<200>() << "MLST OPTS" << tvfs::entry_facts::opts(buffer_operators_.enabled_facts_, opts.size() == 2 ? opts[1] : ""sv);
		return;
	}

	respond<501>() << "Option not understood";
}

FTP_CMD(USER, needs_arg) {
	if (needs_security_before_user_cmd_ && !controller_.is_secure()) {
		respond<503>() << "Use AUTH first.";
		return;
	}

	if (controller_.is_authenticated()) {
		respond<503>() << "Already logged in. QUIT first.";
		return;
	}

	user_ = arg;
	stop(std::move(auth_op_));

	if (timer_id_) {
		stop_timer(timer_id_);
		timer_id_ = 0;
		elapsed_login_time_ += monotonic_clock::now() - start_time_;
	}

	// Let's get the list of auth methods, with this.
	// We'll send the response to the user from within the handle_authenticate_user_response method below.
	controller_.authenticate_user(user_, {}, this);
}

FTP_CMD(PASS, none) {
	if (user_.empty()) {
		respond<503>() << "Login with USER first.";
		return;
	}

	if (controller_.is_authenticated()) {
		respond<503>() << "Already logged in.";
		return;
	}

	if (!auth_op_) {
		// This should never happen, it's a program logic error.
		handle_authenticate_user_response(std::move(auth_op_));
		return;
	}

	if (timer_id_) {
		stop_timer(timer_id_);
		timer_id_ = 0;
		elapsed_login_time_ += monotonic_clock::now() - start_time_;
	}

	if (!next(std::move(auth_op_), {authentication::method::password{std::string(arg)}})) {
		logger_.log_raw(logmsg::error, L"Whoopsie, the authenticator just disappeared!");
		handle_authenticate_user_response(nullptr);
	}
}

authentication::session_user commander::handle_authenticate_user_response(std::unique_ptr<authentication::authenticator::operation> &&op)
{
	if (!CUR_FTP_CMD_IS(PASS)) {
		// Auth hasn't started yet, we should just check whether the methods we support are handled
		// and return a proper response to the USER command.
		//
		// We only deal with the password type at this moment here, and also in case of a error
		// we want the user to get to know about it only after the PASS command has been issued,
		// so either way it's going to be the same message.

		if (login_timeout_) {
			start_time_ = monotonic_clock::now();
			timer_id_ = add_timer(login_timeout_ - elapsed_login_time_, true);
		}

		respond<331>() << "Please, specify the password.";
		auth_op_ = std::move(op);
		return {};
	}

	auto session_user = authentication::session_user(std::move(op), logger_);
	if (session_user) {
		notifier_.notify_user_name(user_);

		if (timer_id_)
			stop_timer(timer_id_);

		if (activity_timeout_)
			timer_id_ = add_timer(activity_timeout_, true);

		if (controller_.get_alpn() == "x-filezilla-ftp"sv)
			controller_.set_data_protection_mode(controller::data_protection_mode::P);

		respond<230>() << "Login successful.";
	}
	else {
		user_ = {};

		if (session_user.error() == authentication::error::internal) {
			respond<530>() << "Login not succeeded because of internal server error. Contact the administrator.";
		}
		else
		if (session_user.error() == authentication::error::user_quota_reached) {
			respond<421>() << "Too many active connections, try again later";
		}
		else {
			respond<530>() << "Login incorrect.";
		}

		if (login_timeout_) {
			start_time_ = monotonic_clock::now();
			timer_id_ = add_timer(login_timeout_ - elapsed_login_time_, true);
		}
	}

	return session_user;
}

FTP_CMD(NOOP, none) {
	respond<200>() << "Noop ok.";
}

FTP_CMD(QUIT, none) {
	respond<221>() << "Goodbye.";
	shutdown();
}

FTP_CMD(AUTH, needs_arg | must_be_last_in_queue) {
	if (!equal_insensitive_ascii(arg, "TLS") && !equal_insensitive_ascii(arg, "SSL")) {
		respond<504>() << "Auth type not supported.";
		return;
	}

	if (controller_.is_secure()) {
		respond<534>() << "Authentication type already set to TLS";
		return;
	}

	controller_.make_secure("234 Using authentication type TLS.\r\n", this);
}

void commander::handle_make_secure_response(bool can_secure)
{
	if (can_secure) {
		act_upon_command_reply(command_reply::positive_completion_reply);
		logger_.log_u(logmsg::reply, L"234 Using authentication type TLS.");
	}
	else {
		respond<504>() << "TLS handshaking failed!";
	}
}

FTP_CMD(PBSZ, needs_arg | needs_security) {
	if (fz::to_integral<int32_t>(arg, -1) < 0)  {
		respond<501>() << "Invalid size";
		return;
	}

	respond<200>() << "PBSZ=0";
}

FTP_CMD(PROT, needs_arg | needs_security) {
	controller::data_protection_mode mode = controller::data_protection_mode::unknown;

	if (arg.size() == 1) switch (fz::toupper_ascii(arg[0])) {
		case 'C': mode = controller::data_protection_mode::C; break;
		case 'E': mode = controller::data_protection_mode::E; break;
		case 'S': mode = controller::data_protection_mode::S; break;
		case 'P': mode = controller::data_protection_mode::P; break;
	}

	auto res = controller_.set_data_protection_mode(mode);

	switch (res) {
		case controller::set_mode_result::unknown:
			respond<504>() << "Unknown protection level"; break;

		case controller::set_mode_result::enabled:
			respond<200>() << "Protection level set to" << mode; break;

		case controller::set_mode_result::not_enabled:
			respond<502>() << "Command not implemented for this authentication type"; break;

		case controller::set_mode_result::not_implemented:
			respond<536>() << "Protection level" << mode << "not supported"; break;

		case controller::set_mode_result::not_allowed:
			respond<534>() << "Protection level" << mode << "not allowed"; break;
	}
}

FTP_CMD(SYST, none) {
	respond<215>() << "UNIX emulated by FileZilla.";
}

FTP_CMD(PWD, needs_auth) {
	respond<257>() << quote(tvfs_.get_current_directory()) <<  "is current directory.";
}

FTP_CMD(CWD, needs_arg | needs_auth) {
	tvfs_.async_set_current_directory(arg, async_receive_ >> [this](result res) {
		if (!res) {
			respond<550>() << strresult(res);
			return;
		}

		respond<250>() << current_cmd_->first << "command successful";
	});
}

FTP_CMD(CDUP, needs_auth) {
	CWD(".."sv);
}

FTP_CMD(ABOR, needs_auth) {
	if (stop_processing_nested_adder() == ECANCELED) {
		auto data_connection_status = controller_.close_data_connection();

		if (data_connection_status == controller::data_connection_status::started)
			respond<426>() << "Data connection closed; transfer aborted.";
		else
		if (cmd_being_aborted_ != commands_.end())
			respond<426>() << "Command aborted.";

		if (data_connection_status != controller::data_connection_status::not_started)
			notifier_.notify_entry_close(1, ECONNABORTED);
	}

	respond<226>() << "ABOR command successful.";
}

FTP_CMD(HELP, none) {
	auto res = respond<214>();

	constexpr std::size_t split_n = 10;
	std::size_t n = 0;

	res << "The following commands are recognized.";

	for (const auto &it: commands_ ) {
		if (n++ % split_n == 0)
			res << endl;

		static constexpr std::string_view spaces = "    ";

		res << std::tuple{it.first, spaces.substr(it.first.size())};
	}

	res << endl << "Help ok.";
}


FTP_CMD(PORT, needs_arg | trim_arg | needs_auth) {
	if (only_allow_epsv_) {
		respond<500>() << "Illegal PORT command, EPSV ALL in effect.";
		return;
	}

	fz::trim_impl(arg, " \t", false, true);
	hostaddress h{arg, hostaddress::format::port_cmd};

	if (h.ipv4()) {
		if (!controller_.set_data_peer_hostaddress(h)) {
			respond<421>() << "Could not create socket.";
		}
		else {
			respond<200>() << "PORT command successful.";
		}
	}
	else {
		respond<500>() << "Illegal PORT command.";
	}
}

FTP_CMD(EPRT, needs_arg | trim_arg | needs_auth) {
	if (only_allow_epsv_) {
		respond<500>() << "Illegal EPRT command, EPSV ALL in effect.";
		return;
	}

	fz::trim_impl(arg, " \t", false, true);
	hostaddress h{arg, hostaddress::format::eprt_cmd};

	if (h.is_valid()) {
		if (h.unknown())
			respond<522>() << "Network protocol not supported, use (1,2)";
		else
		if (!controller_.set_data_peer_hostaddress(h)) {
			respond<421>() << "Could not create socket.";
		}
		else {
			respond<200>() << "EPRT command successful.";
		}
	}
	else {
		respond<500>() << "Illegal EPRT command.";
	}
}

FTP_CMD(PASV, needs_auth) {
	if (only_allow_epsv_) {
		respond<500>() << "Illegal PASV command, EPSV ALL in effect.";
		return;
	}

	if (controller_.get_control_socket_address_family() != fz::address_type::ipv4) {
		respond<500>() << "You are connected using IPv6. PASV is only for IPv4. You have to use the EPSV command instead.";
		return;
	}

	controller_.get_data_local_info(address_type::ipv4, true, *this);
}

FTP_CMD(EPSV, needs_auth) {
	// Yes, we do need to handle it: https://stackoverflow.com/a/55861336/566849
	if (equal_insensitive_ascii(arg, "ALL")) {
		only_allow_epsv_ = true;
		respond<200>() << "EPSV ALL command successful.";
		return;
	}

	address_type family = address_type::unknown;

	if (!arg.empty()) {
		if (arg == "1")
			family = address_type::ipv4;
		else
		if (arg == "2")
			family = address_type::ipv6;
		else {
			respond<522>() << "Network protocol not supported, use (1,2)";
			return;
		}
	}

	controller_.get_data_local_info(family, false, *this);
}

void commander::handle_data_local_info(const std::optional<std::pair<std::string, uint16_t>> &info)
{
	if (!info) {
		respond<425>() << "Cannot prepare for data connection.";
		return;
	}

	auto && port = info->second;

	if (CUR_FTP_CMD_IS(PASV)) {
		auto && ip = fz::replaced_substrings(info->first, ".", ",");
		respond<227>() << "Entering Passive Mode" << std::tuple{'(',ip,',',port/256,',',port%256,')'};
	}
	else {
		respond<229>() << "Entering Extended Passive Mode" << std::tuple{(const char*)"(|||", port, (const char*)"|)"};
	}
}


FTP_CMD(TYPE, needs_arg | needs_auth) {
	const auto &args = fz::strtok_view(arg, " "sv);

	auto old_data_is_binary_ = data_is_binary_;
	std::string_view first_letter;
	std::string_view second_letter;

	if (args.size() >= 1 && args.size() <= 2) {
		if (fz::equal_insensitive_ascii(args[0], first_letter = "A"sv)) {
			data_is_binary_ = false;
			second_letter = "N"sv;
		}
		else
		if (fz::equal_insensitive_ascii(args[0], first_letter = "I"sv)) {
			data_is_binary_ = true;
			second_letter = "N"sv;
		}
		else
		if (fz::equal_insensitive_ascii(args[0], first_letter = "L"sv) && args.size() == 2) {
			data_is_binary_ = true;
			second_letter = "8"sv;
		}

		if (second_letter.empty() || (args.size() == 2 && !fz::equal_insensitive_ascii(args[1], second_letter)))
			goto error_;

		if (args.size() == 1)
			respond<200>() << "Type set to" << first_letter;
		else
			respond<200>() << "Type set to" << first_letter << second_letter;

		return;
	}

error_:
	data_is_binary_ = old_data_is_binary_;
	respond<501>() << "Unsupported type. Supported types are I, I N, A, A N and L 8.";
}

FTP_CMD(STRU, needs_arg | needs_auth) {
	if (equal_insensitive_ascii(arg, "F"))
		respond<200>() << "Using file structure 'File'";
	else
		respond<504>() << "Command not implemented for that parameter";
}

FTP_CMD(MODE, needs_arg | needs_auth) {
	controller::data_mode mode = controller::data_mode::unknown;

	if (arg.size() == 1) switch (fz::toupper_ascii(arg[0])) {
		case 'S': mode = controller::data_mode::S; break;
		case 'Z': mode = controller::data_mode::Z; break;
		case 'C': mode = controller::data_mode::C; break;
		case 'B': mode = controller::data_mode::B; break;
	}

	auto res = controller_.set_data_mode(mode);

	switch (res) {
		case controller::set_mode_result::unknown:
			respond<504>() << "Unknown MODE type"; break;

		case controller::set_mode_result::enabled:
			respond<200>() << "MODE set to" << mode; break;

		case controller::set_mode_result::not_enabled:
			respond<504>() << "MODE" << mode << "not enabled"; break;

		case controller::set_mode_result::not_implemented:
			respond<502>() << "MODE" << mode << "not implemented"; break;

		case controller::set_mode_result::not_allowed:
			respond<504>() << "MODE" << mode << "not allowed"; break;
	}
}

FTP_CMD(MLST, needs_auth)
{
	if (arg.empty()) {
			arg = "."sv;
	}

	tvfs_.async_get_entry(arg, async_receive_ >> [this, arg = std::string(arg)](result result, tvfs::entry &e) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<250>()
			<< "Listing" << arg << endl
			<< tvfs::entry_facts(e, buffer_operators_.enabled_facts_) << endl
			<< "End";
	});
}

FTP_CMD(MLSD, needs_auth | needs_data_connection)
{
	if (arg.empty()) {
			arg = "."sv;
	}

	return tvfs_.async_get_entries(buffer_operators_.entries_iterator_, arg, tvfs::traversal_mode::only_children, async_receive_ >> [this](auto result, const std::string &path) {
		if (!result) {
			if (result.error_ == result.nodir)
				respond<501>() << strresult(result);
			else
				respond<550>() << strresult(result);
			return;
		}

		notifier_.notify_entry_open(1, path, -1);
		controller_.start_data_transfer(buffer_operators_.facts_lister_, this, true);
	});
}

FTP_CMD(LIST, needs_auth | needs_data_connection)
{
#if 0 // Ignore options: they're not RFC-compliant and known servers behave differently about them.
	auto args = strtok_view(arg, ' ');
	args.emplace_back("");

	for (auto a: args) {
		if (a[0] == '-') {
			a.remove_prefix(1);
			if (a.empty()) {
				respond<450>() << "Missing option.";
				return;
			}

			for (auto o: a) {
				switch (o) {
					case 'l':
					case 'L':
					case 'a': break;

					default: respond<450>() << "Unknown option."; return;
				}
			}
		}
		else {
			arg = a;
			break;
		}
	}
#endif

	if (arg.empty()) {
		arg = "."sv;
	}

	return tvfs_.async_get_entries(buffer_operators_.entries_iterator_, arg, tvfs::traversal_mode::autodetect, async_receive_ >> [this](auto result, const std::string &path) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		notifier_.notify_entry_open(1, path, -1);
		controller_.start_data_transfer(buffer_operators_.stats_lister_, this, true);
	});
}

FTP_CMD(NLST, needs_auth | needs_data_connection)
{
	if (arg.empty()) {
		arg = "."sv;
	}

	return tvfs_.async_get_entries(buffer_operators_.entries_iterator_, arg, tvfs::traversal_mode::autodetect, async_receive_ >> [this, maybe_rel_path = util::fs::unix_path(arg)](auto result, const std::string &abs_path) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		if (buffer_operators_.entries_iterator_.get_effective_traversal_mode() == tvfs::traversal_mode::only_children) {
			buffer_operators_.names_prefix_ = maybe_rel_path;
		}
		else {
			buffer_operators_.names_prefix_ = maybe_rel_path.parent();
		}

		if (buffer_operators_.names_prefix_ == "."sv) {
			buffer_operators_.names_prefix_.clear();
		}
		else {
			buffer_operators_.names_prefix_ += '/';
		}

		notifier_.notify_entry_open(1, abs_path, -1);
		controller_.start_data_transfer(buffer_operators_.names_lister_, this, true);
	});
}

void commander::handle_data_transfer(data_transfer_handler::status st, channel::error_type error, std::string_view msg)
{
	if (error) {
		std::string error_string = msg.empty() ? fz::to_utf8(socket_error_description(error)) : std::string(msg);

		notifier_.notify_entry_close(1, error);

		if (st == data_transfer_handler::connecting) {
			if (error == ENOTSOCK) {
				return data_connection_not_setup();
			}
			else {
				respond<425>() << "Unable to build data connection:" << error_string;
			}
		}
		else {
			const char *error_prefix
				= error.source() == channel::error_source::socket
					? "Error while transfering data:"
				: error.source() == channel::error_source::buffer_adder
					? "Error reading from file:"
				: "Error writing to file:";

			respond<425>() << error_prefix << error_string;
		}
	}
	else
	if (st == data_transfer_handler::started) {
		respond<150>() << (msg.empty() ? "Data transfer started" : msg);
	}
	else
	if (st == data_transfer_handler::stopped) {
		notifier_.notify_entry_close(1, error);

		respond<226>() << (msg.empty() ? "Operation successful" : msg);
	}
}

FTP_CMD(STAT, needs_auth) {
	if (arg.empty()) {
		respond<211>()
			<< "FTP Server status:" << endl
			<< "Up and running, yay!" << endl
			<< "End of status";
		return;
	}

	tvfs_.async_get_entries(buffer_operators_.entries_iterator_, arg, tvfs::traversal_mode::autodetect, async_receive_ >> [this](result result, const std::string &path) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		buffer_stream() << "221-Status of " << path << ":\r\n";
		process_nested_adder_until_eof(buffer_operators_.stats_lister_with_prefix_space_, [this](int err) {
			auto stream = buffer_stream();
			stream << "221 End";

			if (err) {
				stream << " *** ERROR (" << err << ")";
			}
			else
			if (CUR_FTP_CMD_IS(ABOR)) {
				stream << " *** ABORTED";
				err = ECANCELED;
			}

			stream << "\r\n";

			act_upon_command_reply(command_reply::positive_completion_reply);

			return err;
		});
	});
}

FTP_CMD(SIZE, needs_arg | needs_auth) {
	tvfs_.async_get_entry(arg, async_receive_ >> [this](result result, auto &e) {
		tvfs::entry_size size = e.size();

		if (result && size < 0)
			result = { result::nofile };

		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<213>() << size;
	});
}

FTP_CMD(RETR, needs_arg | needs_auth | needs_data_connection) {
	tvfs_.async_open_file(buffer_operators_.file_, std::string{arg}, file::mode::reading, rest_size_, async_receive_ >> [&](fz::result res, const std::string &path) {
		if (!res) {
			respond<550>() << strresult(res);
			return;
		}

		notifier_.notify_entry_open(1, path, buffer_operators_.file_->size());
		controller_.start_data_transfer(buffer_operators_.file_reader_, this, data_is_binary_);
	});
}

FTP_CMD(STOR, needs_arg | needs_auth | needs_data_connection) {
	tvfs_.async_open_file(buffer_operators_.file_, std::string{arg}, file::mode::writing, rest_size_, async_receive_ >> [&](fz::result res, const std::string &path) {
		if (!res) {
			respond<550>() << strresult(res);
			return;
		}

		notifier_.notify_entry_open(1, path, buffer_operators_.file_->size());
		controller_.start_data_transfer(buffer_operators_.file_writer_, this, data_is_binary_);
	});
}

FTP_CMD(DELE, needs_arg | needs_auth) {
	tvfs_.async_remove_file(arg, async_receive_ >> [this](auto result, auto) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<250>() << "File deleted successfully.";
	});
}

FTP_CMD(RMD, needs_arg | needs_auth) {
	tvfs_.async_remove_directory(arg, false, async_receive_ >> [this](auto result, auto) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<250>() << "Directory deleted successfully.";
	});
}

FTP_CMD(MKD, needs_arg | needs_auth) {
	tvfs_.async_make_directory(std::string{arg}, async_receive_ >> [&](fz::result result, const std::string &path) {
		if (result) {
			respond<257>() << quote(path) << "created successfully.";
			return;
		}

		respond<550>() << strresult(result);
		return;
	});
}

FTP_CMD(APPE, needs_arg | needs_auth | needs_data_connection) {
	tvfs_.async_open_file(buffer_operators_.file_, std::string{arg}, file::mode::writing, tvfs::append, async_receive_ >> [&](fz::result result, const std::string &path) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		notifier_.notify_entry_open(1, path, buffer_operators_.file_->size());
		controller_.start_data_transfer(buffer_operators_.file_writer_, this, data_is_binary_);
	});
}

FTP_CMD(REST, needs_arg | needs_auth) {
	rest_size_ = to_integral<decltype(rest_size_)>(arg, -1);
	if (rest_size_ < 0)  {
		respond<501>() << "Invalid size";
		return;
	}

	respond<350>() << "Restarting at" << arg;
}

FTP_CMD(MDTM, needs_arg | needs_auth) {
	tvfs_.async_get_entry(arg, async_receive_ >> [this] (auto result, auto &e) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<213>() << tvfs::entry::timeval(e.mtime());
	});
}

FTP_CMD(MFMT, needs_arg | needs_auth) {
	std::string_view timeval;
	std::string_view entry_name;

	using namespace util;
	parseable_range r(arg);

	bool has_two_parameters =
		parse_until_lit(r, timeval, {' '}) && lit(r, ' ') &&
		parse_until_eol(r, entry_name) && !entry_name.empty();

	if (!has_two_parameters) {
		respond<501>() << "Two parameters are required.";
		return;
	}

	auto mtime = tvfs::entry::parse_timeval(timeval);
	if (!mtime) {
		respond<501>() << "Not a valid date";
		return;
	}

	tvfs_.async_set_mtime(entry_name, mtime, async_receive_ >> [this](auto result, tvfs::entry &e) {
		if (!result) {
			respond<550>() << strresult(result);
			return;
		}

		respond<213>() << tvfs::entry_facts(e, tvfs::entry_facts::which::modify);
	});
}

FTP_CMD(RNFR, needs_arg | needs_auth) {
	return tvfs_.async_get_entry(arg, async_receive_ >> [this, arg = std::string(arg)](auto result, tvfs::entry &entry) {
		if (result) {
			if (entry.can_rename()) {
				rename_from_ = arg;

				auto type = entry.type() == tvfs::entry_type::dir ? "Directory" : "File";
				respond<350>() << type << "exists, ready for destination name.";
				return;
			}

			result = { fz::result::noperm };
		}

		respond<550>() << strresult(result);
	});
}

FTP_CMD(RNTO, needs_arg | needs_auth) {
	if (rename_from_.empty()) {
		respond<503>() << "Use RNFR first.";
		return;
	}

	return tvfs_.async_rename(rename_from_, arg, async_receive_ >> [this](auto result, auto) {
		if (!result)
			respond<550>() << strresult(result);
		else
			respond<250>() << "File or directory renamed successfully.";

		rename_from_.clear();
	});
}

FTP_CMD(CLNT, none) {
	respond<200>() << "Don't care.";
}

FTP_CMD(ADAT, needs_auth | needs_security) {
	respond<502>() << "Command not implemented for this authentication type.";
}

FTP_CMD(ALLO, needs_auth) {
	respond<202>() << "No storage allocation neccessary.";
}

FTP_CMD_ALIAS(NOP, NOOP);
FTP_CMD_ALIAS(XCWD, CWD);
FTP_CMD_ALIAS(XRMD, RMD);
FTP_CMD_ALIAS(XMKD, MKD);
FTP_CMD_ALIAS(XPWD, PWD);

#undef FTP_CMD
#undef FTP_CMD_ALIAS

}
