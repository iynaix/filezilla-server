#ifndef FZ_HTTP_BODY_CHUNKER_HPP
#define FZ_HTTP_BODY_CHUNKER_HPP

#include "../buffer_operator/adder.hpp"

namespace fz::http {

class body_chunker: public buffer_operator::adder
{
	static inline constexpr int chunk_size_size_ = sizeof(std::size_t)*2;
	static inline constexpr int chunk_header_size_ = chunk_size_size_ + 2;

public:
	body_chunker(adder_interface &to_chunk, std::size_t chunk_size = 256*1024)
		: to_chunk_(to_chunk)
		, chunk_size_(chunk_size)
	{
		init_chunk(*chunked_buffer_.lock());

		to_chunk_.set_buffer(&chunked_buffer_);
	}

	void set_event_handler(event_handler *eh) override
	{
		adder::set_event_handler(eh);
		to_chunk_.set_event_handler(eh);
	}

	int add_to_buffer() override
	{
		// Is the chunked buffer full?
		if (auto chunk = chunked_buffer_.lock(); chunk->size() >= chunk_size_) {
			// Then finish the chunk
			if (!finish_chunk(*chunk, false)) {
				return EFAULT;
			}

			return 0;
		}

		int res = to_chunk_.add_to_buffer();
		if (res == ENODATA) {
			if (!finish_chunk(*chunked_buffer_.lock(), true)) {
				return EFAULT;
			}
		}
		else
		if (res == ENOBUFS) {
			// The delegate adder's buffer is full, hence write the chunk out and adjust its size,
			// so that next time we won't end up here again needlessy.
			auto chunk = *chunked_buffer_.lock();
			chunk_size_ = chunk.size();

			if (!finish_chunk(chunk, false)) {
				return EFAULT;
			}
		}

		return res;
	}

private:
	void init_chunk(fz::buffer &chunk)
	{
		// Create a placeholder for the actual size
		chunk.append(chunk_size_size_, '0');
		chunk.append("\r\n");
	}

	bool finish_chunk(fz::buffer &chunk, bool eof)
	{
		auto buffer = get_buffer();
		if (!buffer) {
			return false;
		}

		if (chunk.size() < chunk_size_size_+2) {
			// This cannot possibly happen, but we want to play safe.
			return false;
		}

		// Write the correct chunk size where we left the placeholder
		std::snprintf(reinterpret_cast<char *>(chunk.data()), chunk_size_size_+1, "%0*zx", chunk_size_size_, chunk.size() - chunk_header_size_);
		// snprinf overflows with the terminating nul char, but we can just replace that back to the CR
		chunk.data()[chunk_size_size_] = '\r';

		// Finish off the job by appending the chunk terminator
		chunk.append("\r\n");

		if (eof) {
			// EOF reached, terminate the whole thing.
			chunk.append("0\r\n\r\n");
		}

		// Now move that onto the main buffer
		if (buffer->empty()) {
			using namespace std;
			swap(*buffer, chunk);
		}
		else {
			buffer->append(chunk);
			chunk.clear();
		}

		if (!eof) {
			// Init the next chunk.
			init_chunk(chunk);
		}

		return true;
	}

	buffer_operator::unsafe_locking_buffer chunked_buffer_;
	adder_interface &to_chunk_;
	std::size_t chunk_size_;
};

}

#endif // FZ_HTTP_BODY_CHUNKER_HPP
