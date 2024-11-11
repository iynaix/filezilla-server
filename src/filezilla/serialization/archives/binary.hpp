#ifndef FZ_SERIALIZATION_ARCHIVES_BUFFER_HPP
#define FZ_SERIALIZATION_ARCHIVES_BUFFER_HPP

#include <string_view>

#include <libfilezilla/buffer.hpp>

#include "../../serialization/serialization.hpp"

// Define to 1 if it's wanted that the containers sizes be encoded in a "shortened" form, which saves space for small sizes (< 255-sizeof(size_type) bytes).
#define FZ_SERIALIZATION_ARCHIVE_BINARY_SHORTEN_SIZE_TAG 1

namespace fz::serialization {

	namespace buffer_detail {
		template <typename T, typename = std::true_type>
		struct buffer_pointer;

		template <typename T>
		struct buffer_pointer<T, std::integral_constant<bool, std::is_trivially_copyable_v<T>>> {
			buffer_pointer() noexcept {}
			buffer_pointer(buffer &b) noexcept : b_{&b}, o_{b.size()}{}

			struct buffer_reference {
				buffer_reference(buffer_pointer<T> &p) noexcept : p_{p} {}

				buffer_reference &operator =(const T &v) noexcept {
					std::copy_n(reinterpret_cast<const unsigned char *>(&v), sizeof(v), p_.raw());
					return *this;
				}

				operator T() const {
					T v = 0;
					std::copy_n(p_.raw(), sizeof(v), reinterpret_cast<unsigned char *>(&v));
					return v;
				}

				buffer_pointer<T> &p_;
			};

			buffer_reference operator *() noexcept {
				return {*this};
			}

			unsigned char *raw() noexcept {
				return b_->get() + o_;
			}

		private:
			buffer *b_{};
			std::size_t o_{};
		};
	}



	class binary_output_archive: public archive_output<binary_output_archive> {
	public:
		binary_output_archive(buffer &buffer)
			: buffer_{buffer}
			, orig_size_(buffer_.size())
		{
			(*this)(is_little_endian(), payload_size_);
		}

		~binary_output_archive()
		{
			if (!error_) {
				const auto buffer_end = buffer_.get() + buffer_.size();

				auto payload_size = buffer_end - payload_size_.raw();
				payload_size -= sizeof(size_type);

				if (payload_size > max_size)
					error(ENOSPC);
				else
					*payload_size_ = size_type(payload_size);
			}

			if (error_) {
				// If there was an error, just undo whatever was added to the buffer.
				buffer_.resize(orig_size_);
			}
		}

		template <typename T>
		static fz::buffer encode(const T &v);

	private:
		friend access;

		using archive_output<binary_output_archive>::process;

		template <typename T>
		void process(buffer_detail::buffer_pointer<T> &p) {
			p = buffer_detail::buffer_pointer<T>{buffer_};
			buffer_.get(sizeof(T));
			buffer_.add(sizeof(T));
		}

	public:
		template <typename T, std::enable_if_t<std::is_trivially_copyable_v<T>>* = nullptr>
		void processBinary(const T *data, std::size_t num)
		{
			buffer_.append(reinterpret_cast<const unsigned char *>(data), num*sizeof(T));
			error_ = 0;
		}

	private:
		buffer_detail::buffer_pointer<size_type> payload_size_;
		buffer &buffer_;
		size_t orig_size_;
	};

	class binary_input_archive: public archive_input<binary_input_archive> {
	public:
		binary_input_archive(buffer &buffer, size_type max_payload_size = max_size)
			: buffer_{buffer}
			, biew_{buffer_.get(), buffer_.size()}
		{
			FZ_SERIALIZATION_DEBUG_LOG(L"BIEW (%d): [%s]", biew_.size(), fz::hex_encode<std::string>(biew_));
			auto orig_size = biew_.size();

			if (std::uint8_t data_is_little_endian = false; (*this)(data_is_little_endian)) {
				needs_endianess_conversion_ = data_is_little_endian != is_little_endian();

				if ((*this)(payload_size_)) {
					FZ_SERIALIZATION_DEBUG_LOG(L"PAYLOAD (%d)", size_t(payload_size_));

					if (payload_size_ > max_payload_size) {
						payload_size_ = size_type(0);
						error_ = EFBIG;
					}
					else
					if (biew_.size() < payload_size_) {
						payload_size_ = size_type(0);
						error_ = ENODATA;
					}
					else {
						buffer_.consume(orig_size-biew_.size());
						biew_ = std::basic_string_view<unsigned char>(buffer_.get(), size_t(payload_size_));
					}
				}
				else
					FZ_SERIALIZATION_DEBUG_LOG(L"LOADED: COULDN'T PARSE (2)");
			}
			else
				FZ_SERIALIZATION_DEBUG_LOG(L"LOADED: COULDN'T PARSE (1)");
		}

		template <typename T>
		static std::optional<T> decode(fz::buffer buf);

		template <typename T>
		static std::optional<T> decode(std::string_view v);

		template <typename T>
		static std::optional<T> decode(const std::vector<uint8_t> &v);

		~binary_input_archive()
		{
			buffer_.consume(size_t(payload_size_));
		}

		template <typename T, std::enable_if_t<std::is_trivially_copyable_v<T>>* = nullptr>
		void processBinary(T *data, std::size_t num) {
			auto size = num*sizeof(T);

			if (biew_.size() < size) {
				error_ = ENODATA;
				return;
			}

			std::copy_n(biew_.data(), size, reinterpret_cast<unsigned char *>(data));

			if constexpr (sizeof(T) > 1 && (std::is_integral_v<T> || std::is_enum_v<T>)) {
				if (needs_endianess_conversion_) {
					for (auto ptr = reinterpret_cast<std::uint8_t*>(data); num--; ptr += sizeof(T)) {
						for (std::size_t i = 0, end = sizeof(T)/2; i < end; ++i )
							std::swap(ptr[i], ptr[sizeof(T) - i - 1]);
					}
				}
			}

			biew_.remove_prefix(size);
		}

	private:
		buffer &buffer_;
		std::basic_string_view<unsigned char> biew_;
		size_type payload_size_{};
		bool needs_endianess_conversion_{};
	};

	template <typename Archive, typename T, std::enable_if_t<std::is_arithmetic_v<T>>* = nullptr>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	serialize(Archive &ar, binary_data<T> &bd) {
		ar.processBinary(bd.data, bd.size);
	}

	template <typename Archive, typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<bool, std::decay_t<T>>>* = nullptr>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	serialize(Archive &ar, T &i) {
		ar.processBinary(&i, 1);
	}

	//! Bool types are saved as uint8_t by the binary archive
	template <typename T, std::enable_if_t<std::is_same_v<bool, std::decay_t<T>>>* = nullptr>
	inline std::uint8_t save_minimal(const binary_output_archive &, const T &b) {
		return static_cast<std::uint8_t>(b);
	}

	//! Bool types are loaded as uint8_t by the binary archive
	template <typename T, std::enable_if_t<std::is_same_v<bool, std::decay_t<T>>>* = nullptr>
	inline void load_minimal(const binary_input_archive &, T &b, std::uint8_t v) {
		b = static_cast<bool>(v);
	}

#if FZ_SERIALIZATION_ARCHIVE_BINARY_SHORTEN_SIZE_TAG
	// This is certainly slower than the naif approach, but it saves lots of space.
	template <typename Archive, typename T>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	save(Archive &ar, const size_tag<T> &s)
	{
		if (s.value > max_size)
			return ar.error(ENOSPC);

		constexpr std::uint8_t small_size = 255-sizeof(size_type);

		uint8_t buf[1 + sizeof(size_type)];

		buf[0] = s.value < small_size ? std::uint8_t(s.value) : small_size;
		auto size = s.value - buf[0];

		auto cur = &buf[1];
		while (size) {
			*cur++ = size & 0xFF;
			size >>= 8;
		}

		buf[0] += cur - &buf[1];

		ar.processBinary(&buf[0], cur-&buf[0]);
	}

	template <typename Archive, typename T>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	load(Archive &ar, const size_tag<T> &s)
	{
		constexpr std::uint8_t small_size = 255-sizeof(size_type);
		std::uint8_t buf[1 + sizeof(size_type)];

		if (ar.processBinary(&buf[0], 1); ar) {
			if (auto count = buf[0]; count < small_size)
				s.value = count;
			else {
				count -= small_size;
				if (ar.processBinary(&buf[1], count); ar) {
					s.value = 0;

					while (count) {
						s.value <<= 8;
						s.value |= buf[count--];
					}

					s.value += small_size;
				}
			}
		}
	}
#else
	template <typename T>
	size_type save_minimal(const binary_output_archive &ar, const size_tag<T> &s, binary_output_archive::error_t &e) {
		if (s.value > max_size) {
			e = ar.error(ENOSPC);
			return size_type(0);
		}

		return size_type(s.value);
	}

	template <typename T>
	void load_minimal(const binary_input_archive &, const size_tag<T> &s, const size_type &size, binary_input_archive::error_t &) {
		s.value = std::decay_t<T>(size);
	}
#endif

	template <typename Archive, typename T, std::size_t N>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	serialize(Archive &ar, nvp<T, N> &v) {
		ar(v.value);
	}

	template <typename Archive, typename T>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	serialize(Archive &ar, value_info<T> &v) {
		ar(v.value);
	}

	template <typename Archive, typename Key, typename Value>
	trait::restrict_t<Archive, binary_input_archive, binary_output_archive>
	serialize(Archive &ar, map_item<Key, Value> &item)
	{
		ar(item.key, item.value);
	}

	template <typename Archive, typename... Ts>
	trait::restrict_t<Archive, binary_input_archive>
	load(Archive &ar, std::basic_string<wchar_t, Ts...> &wstr) {
		if (std::string utf8; ar(utf8))
			wstr = fz::to_wstring_from_utf8(utf8);
	}

	template <typename Archive, typename... Ts>
	trait::restrict_t<Archive, binary_output_archive>
	save(Archive &ar, const std::basic_string<wchar_t, Ts...> &wstr) {
		ar(fz::to_utf8(wstr));
	}

	/************************/

	template<typename T>
	inline buffer binary_output_archive::encode(const T &v)
	{
		buffer buf;
		binary_output_archive ar(buf);
		if (!ar(v)) {
			buf.clear();
		}

		return buf;
	}

	template<typename T>
	inline std::optional<T> binary_input_archive::decode(buffer buf)
	{
		binary_input_archive ar(buf);
		if (T v; ar(v)) {
			return std::move(v);
		}

		return std::nullopt;
	}

	template<typename T>
	inline std::optional<T> binary_input_archive::decode(std::string_view v)
	{
		buffer buf;
		buf.append(v);

		return decode<T>(std::move(buf));
	}

	template<typename T>
	inline std::optional<T> binary_input_archive::decode(const std::vector<uint8_t> &v)
	{
		buffer buf;
		buf.append(v);

		return decode<T>(std::move(buf));
	}

}

FZ_SERIALIZATION_LINK_ARCHIVES(binary_input_archive, binary_output_archive)

#endif // BUFFER_SERIALIZER_HPP
