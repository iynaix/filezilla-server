#ifndef FZ_BUFFER_OPERATOR_LINE_CONSUMER_HPP
#define FZ_BUFFER_OPERATOR_LINE_CONSUMER_HPP

#include "../buffer_operator/consumer.hpp"

namespace fz {

	enum class buffer_line_eol: std::uint8_t {
		lf, cr_lf
	};

}

namespace fz::buffer_operator {

	template <buffer_line_eol EOL>
	class line_consumer: public consumer {
	public:
		using buffer_string_view = std::basic_string_view<std::decay_t<decltype(*std::declval<buffer>().get())>>;

		line_consumer(size_t max_line_size): max_line_size_{max_line_size}{}

	protected:
		int consume_buffer() override {
			auto buffer = get_buffer();
			if (!buffer)
				return EFAULT;

			auto biew = buffer_string_view{buffer->get(), buffer->size()};

			bool is_eol{};
			std::size_t eol_pos{};
			std::size_t eol_size{};

			static_assert(EOL == buffer_line_eol::cr_lf || EOL == buffer_line_eol::lf);

			if constexpr (EOL == buffer_line_eol::cr_lf) {
				static const unsigned char sentinel_chars[2] = { '\r', '\0' };
				static const std::basic_string_view<unsigned char> sentinel(sentinel_chars, 2);

				eol_size = 2;
				eol_pos = biew.find_first_of(sentinel);

				if (eol_pos < biew.size()-1) {
					if (biew[eol_pos] == '\0')
						return EINVAL;

					is_eol = biew[eol_pos+1] == '\n';
				}
			}
			else
			if constexpr (EOL == buffer_line_eol::lf) {
				static const unsigned char sentinel_chars[2] = { '\n', '\0' };
				static const std::basic_string_view<unsigned char> sentinel(sentinel_chars, 2);

				eol_size = 1;
				eol_pos = biew.find_first_of(sentinel);

				if (eol_pos < biew.size()) {
					if (biew[eol_pos] == '\0')
						return EINVAL;

					is_eol = biew[eol_pos] == '\n';
				}
			}

			if (is_eol) {
				auto consumable_size = eol_pos+eol_size;

				int ret = process_buffer_line(biew.substr(0, eol_pos), consumable_size < buffer->size());
				if (ret == 0)
					buffer->consume(consumable_size);

				return ret;
			}

			if (biew.size() >= max_line_size_)
				return ENOBUFS;

			return ENODATA;
		}

	protected:
		virtual int process_buffer_line(buffer_string_view line, bool there_is_more_data_to_come) = 0;

	private:
		size_t max_line_size_;
	};

}

#endif // FZ_BUFFER_OPERATOR_LINE_CONSUMER_HPP
