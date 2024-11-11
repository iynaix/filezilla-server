#include <string_view>
#include <iostream>
#include <cstring>

#include <libfilezilla/socket.hpp>
#include <libfilezilla/thread_pool.hpp>

#include "../../src/filezilla/pipe.hpp"
#include "../../src/filezilla/buffer_operator/socket_adapter.hpp"
#include "../../src/filezilla/buffer_operator/file_writer.hpp"
#include "../../src/filezilla/buffer_operator/file_reader.hpp"

#include "../../src/filezilla/ftp/ascii_layer.hpp"


[[noreturn]] void die(int err) {
	if (err) std::cerr << "Error: " << std::strerror(err) << std::endl;
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
	fz::event_loop server_loop {fz::event_loop::threadless};

	struct application: fz::event_handler {
		application(fz::event_loop &el, std::basic_string_view<char *> args)
			: fz::event_handler{el},
			  args_{args}
		{
			std::size_t numopts = 0;

			for (auto a: args_) {
				if (a && a[0] == '-') {
					std::string_view s{a+1};
					if (s.empty()) die(EINVAL);

					for (auto o: s) {
						switch (o) {
							case 'l': is_server_ = true; break;
							case 'a': needs_ascii_ = true; break;
							default: die(EINVAL);
						}
					}

					++numopts;
				}
			}

			args_.remove_prefix(numopts);

			int err = args_.size() == 3 ? 0 : EINVAL;

			if (!err) {
				if (f_.open(fz::to_native(args_[2]), is_server_ ? fz::file::mode::reading : fz::file::mode::writing, fz::file::creation_flags::empty)) {
					if (is_server_) {
						l_ = std::make_unique<fz::listen_socket>(pool_, this);
						err = !l_->bind(args_[0]) ? EBADF : l_->listen(fz::address_type::unknown, std::atoi(args_[1]));
					}
					else {
						s_ = std::make_unique<fz::socket>(pool_, this);
						err = s_->connect(fz::to_native(args_[0]), unsigned(std::atoi(args_[1])));
					}
				}
				else {
					err = EIO;
				}
			}

			if (err) die(err);
		}

		~application() override {
			remove_handler();
		}

		void operator()(const fz::event_base &event) override {
			fz::dispatch<
				fz::socket_event,
				fz::pipe::done_event
			>(event, this,
				&application::on_socket_event,
				&application::on_pipe_done_event
			);
		}


		void on_socket_event(fz::socket_event_source *s, fz::socket_event_flag type, int error) {
			if (error) die(error);

			if (type == fz::socket_event_flag::connection) {
				if (l_) {
					s_ = l_->accept(error);
					if (!s) die(error);
				}

				if (s_) {
					if (needs_ascii_)
						ascii_layer_ = std::make_unique<fz::ftp::ascii_layer>(this, *s_);

					if (is_server_) {
						p_.set_consumer(&sa_);
						p_.set_adder(&fr_);
					}
					else {
						p_.set_consumer(&fw_);
						p_.set_adder(&sa_);
					}

					sa_.set_socket(ascii_layer_ ? ascii_layer_.get() : s_.get());
				}
			}
		}

		void on_pipe_done_event(fz::pipe &, fz::pipe::error_type error) {
			if (error) die(error);
			event_loop_.stop();
		}

		fz::thread_pool pool_;

		fz::file f_;

		fz::pipe p_{*this, 5, false};
		fz::buffer_operator::socket_adapter sa_{*this, 128*1024};
		fz::buffer_operator::file_reader fr_{f_, 128*1024};
		fz::buffer_operator::file_writer fw_{f_};

		std::unique_ptr<fz::socket_interface> s_;
		std::unique_ptr<fz::socket_interface> ascii_layer_;
		std::unique_ptr<fz::listen_socket> l_;

		std::basic_string_view<char *> args_;

		bool is_server_{};
		bool needs_ascii_{};
	} app {server_loop, std::basic_string_view{argv+(argc>0), std::size_t(argc-(argc>0))}};

	server_loop.run();

	return EXIT_SUCCESS;
}
