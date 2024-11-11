#include <iomanip>
#include <string_view>
#include <iostream>
#include <cstring>
#include <map>

#include <libfilezilla/socket.hpp>
#include <libfilezilla/thread_pool.hpp>

#include "../../src/filezilla/buffer_operator/socket_adapter.hpp"
#include "../../src/filezilla/buffer_operator/line_consumer.hpp"
#include "../../src/filezilla/pipe.hpp"
#include "../../src/filezilla/logger/stdio.hpp"
#include "../../src/filezilla/logger/type.hpp"

#include "../../src/server/administrator.hpp"
#include "../../src/filezilla/rmp/dispatch.hpp"

[[noreturn]] void die(int err) {
	if (err) std::cerr << "Error: " << std::strerror(err) << std::endl;
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
	fz::event_loop server_loop {fz::event_loop::threadless};

	struct application: fz::event_handler, administration::engine::visitor<application>, fz::buffer_operator::line_consumer<fz::buffer_line_eol::lf> {
		application(fz::event_loop &el, std::basic_string_view<char *> args)
			: fz::event_handler{el},
			  visitor{*this},
			  line_consumer<fz::buffer_line_eol::lf>{100},
			  c_{pool_, el, *this, logger_},
			  pipe_{*this, 5, false},
			  socket_reader_{*this, 128*1024},
			  args_{args}
		{
			int err = args_.size() == 2 ? 0 : EINVAL;

			if (!err)
				c_.connect({args_[0], static_cast<unsigned int>(std::atoi(args[1]))});

			stdin_ = fz::socket::from_descriptor(fz::socket_descriptor{0}, pool_, err);

			if (err) die(err);

			pipe_.set_adder(&socket_reader_);
			pipe_.set_consumer(this);

			socket_reader_.set_socket(stdin_.get());
		}

		~application() override {
			remove_handler();
		}

		int process_buffer_line(buffer_string_view line, bool) override {
			auto id = fz::to_integral<uint64_t>(line);
			c_.template send<administration::end_sessions>(id);
			return 0;
		}

		void certificate_verification(administration::engine::session &s, fz::tls_session_info &&) override
		{
			s.set_certificate_verification_result(true);
		}

		void operator()(const fz::event_base &e) override {
			fz::dispatch<fz::pipe::done_event>(e, [](fz::pipe &, fz::pipe::error_type err){
				if (err) die(err.error());
			});
		}

		struct session {
			fz::datetime start_time{};

			fz::file::mode transfer_mode_;

			struct transfer_info {
				fz::duration start_time{};
				std::int64_t millis{};
				std::int64_t total_amount_{};
				std::int64_t previous_total_amount{};
				std::int64_t previous_second_amount{};
			} transfer_[2]{};
		};

		std::map<uint64_t, session> sessions_;

		/*
		// The following shows that any_message can be used as a fall-through OR to implement one's own dispatching mechanism.
		// The syntax is purposedly analogous to the one used for handling fz::basic_event's
		void operator()(const administration::engine::any_message &any) {
			std::cerr << "!!! any: index: " << any.index() << std::endl;

			fz::rmp::dispatch<administration::session::start>(any, [&](auto && session_id, auto && start_time, auto && peer_host, fz::address_type) {
				sessions_[session_id].start_time = std::move(start_time);

				std::cerr << "!!! session_start: id: " << session_id << ", time: " << start_time.get_rfc822() << ", peer:" << peer_host << std::endl;
			});
		}*/

		//void operator()(session_start && v) {
		//    auto && [session_id, start_time, peer_host] = v.tuple();
		//    sessions_[session_id].start_time = std::move(start_time);
		//
		//    std::cerr << "!!! session_start: id: " << session_id << ", time: " << start_time.get_rfc822() << ", peer:" << peer_host << std::endl;
		//}

		void operator()(administration::session::stop && v) {
			auto && [session_id, since_start] = v.tuple();

			std::cerr << "!!! session_stop: id: " << session_id << ", time: " << (sessions_[session_id].start_time+since_start).get_rfc822() << std::endl;
			sessions_.erase(session_id);
		}

		void operator()(administration::session::user_name && v) {
			auto && [session_id, since_start, user_name] = v.tuple();

			std::cerr << "!!! user_name: id: " << session_id << ", time: " << (sessions_[session_id].start_time+since_start).get_rfc822()
					  << ", name: " << user_name << std::endl;
		}

		void operator()(administration::log &&v) {
			auto && [dt, session_id, type, module, msg] = std::move(v).tuple();
			std::cerr << "[SERVER LOG] " << dt.format("%Y-%m-%dT%H:%M:%S.", fz::datetime::zone::utc) << std::setw(3) << std::setfill('0') << dt.get_milliseconds() << "Z"
					  << fz::logger::type2str<std::string_view>(type) << " "
					  << fz::to_utf8(msg) << "\n"
					  << std::setw(0) << std::setfill(' ');
		}

		void log_progress(std::uint64_t session_id, fz::duration since_start, std::int64_t total_amount, std::int64_t delta_update) {
			auto &session = sessions_[session_id];
			auto &t = session.transfer_[session.transfer_mode_];
			auto delta_time = (since_start-t.start_time);
			auto delta_millis = delta_time.get_milliseconds();

			t.total_amount_ = total_amount;
			auto amount = total_amount - t.previous_total_amount;

			if (delta_millis > t.millis + delta_update) {
				auto delta_amount = amount - t.previous_second_amount;
				auto delta_secs = delta_millis/1000;

				std::cerr << "!!! progress: id: " << session_id << ", time: " << (session.start_time+since_start).get_rfc822()
						  << ", mode: " << session.transfer_mode_ << ", previous: " << t.previous_total_amount << ", amount: " << amount
						  << ", duration: " << delta_secs/60/10 << delta_secs/60%10 << ":" << (delta_secs%60)/10 << (delta_secs%60)%10
						  << ", avg speed: " << double(amount)*1000/delta_millis/1024/1024 << "MiB/s"
						  << ", inst speed: " << double(delta_amount)*1000/(delta_millis-t.millis)/1024/1024 << "MiB/s" << std::endl;

				t.millis = delta_millis;
				t.previous_second_amount = amount;
			}
		}

		void operator()(administration::end_sessions::response && v) {
			std::cerr << "!!! end_sessions::response: ok: " << (bool)v << std::endl;
		}

		fz::thread_pool pool_;

		administration::engine::client c_;
		fz::pipe pipe_;
		fz::buffer_operator::socket_adapter socket_reader_;
		fz::logger::stdio logger_{stderr};

		std::basic_string_view<char *> args_;

		std::unique_ptr<fz::socket> stdin_;
	} app {server_loop, std::basic_string_view{argv+(argc>0), std::size_t(argc-(argc>0))}};

	server_loop.run();

	return EXIT_SUCCESS;
}
