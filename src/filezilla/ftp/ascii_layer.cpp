#include <libfilezilla/util.hpp>

#include "ascii_layer.hpp"

namespace fz::ftp {

ascii_layer::ascii_layer(event_handler *handler, socket_interface &next_layer)
	: socket_layer{handler, next_layer, true}
{}

int ascii_layer::read(void *buffer, unsigned int size, int &error)
{
#if 1
	static constexpr const char crlf_begin[] = { '\r', '\n' };
	static constexpr const char *crlf_end = crlf_begin + 2;

	char *begin = reinterpret_cast<char*>(buffer);
	char *end;
	int read;

	do {
		if (tmp_read_.has_value() && size > 0) {
			*begin = *tmp_read_;

			if (size > 1) {
				read = next_layer_.read(begin + 1, size - 1, error);

				if (read < 0)
					return read;

				if (read == 0) {
					tmp_read_.reset();
					return 1;
				}

				read += 1;
			}
			else {
				char ch;
				read = next_layer_.read(&ch, 1, error);

				if (read < 0)
					return read;

				if (read == 0) {
					tmp_read_.reset();
				}
				else
				if (ch == '\n' && *begin == '\r') {
					*begin = '\n';

					tmp_read_.reset();
				}
				else
					tmp_read_.emplace(ch);

				return 1;
			}

		}
		else {
			read = next_layer_.read(begin, size, error);

			if (read <= 0)
				return read;
		}

		// Invariant: read > 0

		end = begin + read;

		// Use std algos to eliminate the excess CR's.

		int crlf_count = 0;
		if (auto crlf_it1 = std::search(begin, end, crlf_begin, crlf_end); crlf_it1 != end) {
			do {
				auto crlf_it2 = std::search(crlf_it1+2, end, crlf_begin, crlf_end);
				std::copy(crlf_it1+1, crlf_it2, crlf_it1-crlf_count++);
				crlf_it1 = crlf_it2;
			} while (crlf_it1 != end);
		}

		read -= crlf_count;

		// Invariant: read > 0, still, because crlf_count is *at most* equal to integer read/2.

		read -= 1;
		tmp_read_.emplace(begin[read]);

		// If the only byte we've produced ended up in the side buffer tmp_read_, then we ought to put it back into
		// the user buffer and try to get more data from next_layer_, or an EOF. Either way, read will be >= 1 and we won't repeat this cycle yet again.
	}  while (read == 0);

	return signed(read);
#else
	int res;
	do {
		res = next_layer_.read(buffer, size, error);
		if (res < 0)
			return res;

		auto begin = static_cast<char *>(buffer);
		auto end = begin+res;

		auto write_cur = begin;
		auto read_cur = begin;

		if (tmp_read_) {
			if (res == 0 && size > 0) {
				// EOF was reached while having something in tmp_read_
				*write_cur = *tmp_read_;
				tmp_read_ = std::nullopt;
				return 1;
			}

			has_tmp_read:
			while (read_cur < end) {
				auto &ch = *write_cur++ = *read_cur++;

				if (ch == '\n' && *tmp_read_ == '\r') {
					tmp_read_ = std::nullopt;
					goto hasnt_tmp_read;
				}

				std::swap(*tmp_read_, ch);
			}
		}
		else {
			hasnt_tmp_read:
			while (read_cur < end) {
				auto ch = *read_cur++;

				if (ch == '\r') {
					tmp_read_ = '\r';
					goto has_tmp_read;
				}

				*write_cur++ = ch;
			}
		}

		res = signed(write_cur - begin);
	} while (res == 0 && tmp_read_);

	return res;
#endif
}

int ascii_layer::write(const void *from, unsigned int size, int &error)
{
	// next_layer_.write() might fail or write less than the requested amount.
	// In that case, we'd have wasted computation proportional to the number of bytes converted but not written out.
	// Putting a cap on size, thus, allows us to minimize the number of wasted bytes. It can't be too low, though, or else we'd end up
	// calling next_layer_.write() too many times.
	//
	// An alternative approach would be to store the converted bytes into a buffer for later reuse, but that would complicate a lot this otherwise very simple ascii layer.
	constexpr unsigned int size_cap = 128*1024;

	if (size > size_cap)
		size = size_cap;

	auto read_begin = reinterpret_cast<const unsigned char *>(from);
	auto read_end = read_begin+size;
	auto read_cur = read_begin;

	auto write_begin = tmp_write_.get(size*2);
	auto write_cur = write_begin;

	crpos_.clear();

	while (read_cur != read_end) {
		auto ch = *read_cur++;

		if (ch == '\n' && last_written_ch_ != '\r') {
			crpos_.emplace_back(write_cur-write_begin);
			*write_cur++ = '\r';
		}

		last_written_ch_ = *write_cur++ = ch;
	}

	auto size_w = write_cur-write_begin;

	int written = next_layer_.write(tmp_write_.get(), unsigned(size_w), error);
	if (written <= 0)
		return written;

	// If the number of bytes written from the buffer is less than the number of bytes in the buffer, then
	// we need to report back to the caller an amount of bytes written which is less than what the caller asked us to write.
	// Since we might have written out more CR's than there were in the initial data, the amount we need to report back to the caller is:
	// the number of bytes written minus the number of CR's written.
	if (written < size_w) {
		last_written_ch_ = *--write_cur;

		// Find the largest element in crpos whose value <= written. This element's index+1 will be equal to the number of CR's written out.
		auto it = std::upper_bound(crpos_.crbegin(), crpos_.crend(), written, [](int a, int b) { return a >= b; });

		// the distance between the begin of the vector and the base of a reverse iterator into the vector already contains the +1.
		written -= std::distance(crpos_.cbegin(), it.base());

		size = unsigned(written);
	}

	return signed(size);
}

int ascii_layer::connect(const native_string &host, unsigned int port, address_type family)
{
	return next_layer_.connect(host, port, family);
}

socket_state ascii_layer::get_state() const
{
	return next_layer_.get_state();
}

int ascii_layer::shutdown()
{
	return next_layer_.shutdown();
}

}
