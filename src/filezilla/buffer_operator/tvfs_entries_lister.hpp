#ifndef FZ_FTP_TVFS_ENTRIES_LISTER_HPP
#define FZ_FTP_TVFS_ENTRIES_LISTER_HPP

#include <libfilezilla/event_handler.hpp>

#include "../tvfs/entry.hpp"
#include "../buffer_operator/adder.hpp"

namespace fz::buffer_operator {


	template <typename EntryStreamer>
	class with_prefix
	{
	public:
		template <typename... Args>
		with_prefix(tvfs::entry &entry, std::string_view prefix, Args &&... args)
			: st_(entry, std::forward<Args>(args)...)
			, px_(prefix)
		{}

		void operator()(util::buffer_streamer &bs) const
		{
			bs << px_ << st_;
		}

	private:
		EntryStreamer st_;
		std::string_view px_;
	};

	template <typename EntryStreamer>
	class with_suffix
	{
	public:
		template <typename... Args>
		with_suffix(tvfs::entry &entry, std::string_view suffix, Args &&... args)
			: st_(entry, std::forward<Args>(args)...)
			, sx_(suffix)
		{}

		void operator()(util::buffer_streamer &bs) const
		{
			bs << st_ << sx_;
		}

	private:
		EntryStreamer st_;
		std::string_view sx_;
	};

	template <typename EntryStreamer, typename... Args>
	class tvfs_entries_lister: public adder {
	public:
		tvfs_entries_lister(event_loop &loop, tvfs::entries_iterator &it, Args... args)
			: h_(loop)
			, it_{it}
			, args_{args...}
		{}

		int add_to_buffer() override
		{
			if (!it_.has_next())
				return ENODATA;

			it_.async_next(async_receive(h_) >> [this](auto result, tvfs::entry &entry) {
				if (!result) {
					adder::send_event(EINVAL);
					return;
				}

				std::apply([&](auto& ...args) {
					auto buffer = get_buffer();
					if (!buffer) {
						adder::send_event(EFAULT);
						return;
					}

					util::buffer_streamer(*buffer) << EntryStreamer(entry, args...);

					adder::send_event(0);
				}, args_);
			});

			return EAGAIN;
		}

	private:

		async_handler h_;
		tvfs::entries_iterator &it_;
		std::tuple<Args...> args_;
	};

}

#endif // FZ_FTP_TVFS_ENTRIES_LISTER_HPP
