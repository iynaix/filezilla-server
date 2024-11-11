#ifndef BUFFER_STREAMER_HPP
#define BUFFER_STREAMER_HPP

#include <array>
#include <functional>

#include <libfilezilla/string.hpp>
#include <libfilezilla/buffer.hpp>

#include "integral_ops.hpp"

namespace fz::util {

	class buffer_streamer {
	public:
		buffer_streamer(buffer &buf, std::function<void()> fin = {}): buf_{buf}, fin_{std::move(fin)}{}
		~buffer_streamer() { if (fin_) fin_(); }

		buffer_streamer(const buffer_streamer &) = delete;
		buffer_streamer(buffer_streamer &&) = delete;

		template <typename T>
		buffer_streamer &operator <<(T && v)
		{
			return write(std::forward<T>(v));
		}

		template <typename ...T>
		buffer_streamer &operator()(T &&... v)
		{
			static_cast<void>((write(std::forward<T>(v)), ...));
			return *this;
		}

		struct ref {
			ref(){}
			explicit ref(buffer &b): b_{&b}, o_{b.size()}{}

			unsigned char* operator &() noexcept { return b_->get() + o_; }
			const unsigned char * operator &() const noexcept { return b_->get() + o_; }

			unsigned char &operator[](std::size_t i) noexcept { return operator&()[i]; }
			unsigned char operator[](std::size_t i) const noexcept { return operator&()[i]; }

		private:
			buffer *b_{};
			std::size_t o_{};
		};

		unsigned char *cur() const
		{
			return buf_.get() + buf_.size();
		}

		template <typename Integral, std::enable_if_t<std::is_integral_v<Integral>>* = nullptr>
		static
		auto dec(Integral i, unsigned char min_width = 0, unsigned char fill = ' ') {
			return [=](buffer_streamer &bs) {
				bs.write(i, 10, min_width, fill);
			};
		}

		template <typename Integral, std::enable_if_t<std::is_integral_v<Integral>>* = nullptr>
		static
		auto hex(Integral i, unsigned char min_width = 0, unsigned char fill = ' ') {
			return [=](buffer_streamer &bs) {
				bs.write(i, 16, min_width, fill);
			};
		}

		template <typename Integral, std::enable_if_t<std::is_integral_v<Integral>>* = nullptr>
		static
		auto oct(Integral i, unsigned char min_width = 0, unsigned char fill = ' ') {
			return [=](buffer_streamer &bs){
				bs.write(i, 8, min_width, fill);
			};
		}

	private:
		buffer_streamer &write(const char *begin, const char *end)
		{
			buf_.append(reinterpret_cast<const unsigned char *>(begin), static_cast<std::size_t>(end-begin));
			return *this;
		}

		template <typename Iterator>
		buffer_streamer &
		write(Iterator begin, Iterator end)
		{
			for (;begin != end; ++begin)
				*this << *begin;

			return *this;
		}

		template <typename Func, std::enable_if_t<std::is_invocable_r_v<void, Func, buffer_streamer &>>* = nullptr>
		buffer_streamer &write(const Func &f)
		{
			f(*this);
			return *this;
		}

		buffer_streamer &write(const char *s)
		{
			return write(s, s+std::char_traits<char>::length(s));
		}

		buffer_streamer &write(std::string_view sv)
		{
			return write(sv.data(), sv.data()+sv.size());
		}

		buffer_streamer &write(std::wstring_view sv)
		{
			const auto &utf8 = fz::to_utf8(sv);
			return write(utf8.data(), utf8.data()+utf8.size());
		}

		buffer_streamer &write(const std::string &s)
		{
			return write(s.data(), s.data()+s.size());
		}

		buffer_streamer &write(const std::wstring &ws)
		{
			const auto &utf8 = fz::to_utf8(ws);
			return write(utf8.data(), utf8.data()+utf8.size());
		}

		template <size_t N>
		buffer_streamer &write(const char (&a)[N])
		{
			return write(std::string_view(a, a[N-1] == '\0' ? N-1 : N));
		}

		template <size_t N>
		buffer_streamer &write(const wchar_t (&a)[N])
		{
			return write(std::wstring_view(a, a[N-1] == '\0' ? N-1 : N));
		}

		template <typename Range>
		buffer_streamer &write(const Range &range, decltype(std::begin(range))* = nullptr)
		{
			return write(std::begin(range), std::end(range));
		}

		buffer_streamer &write(fz::buffer && moved_buffer)
		{
			// We swap if possible, or else we clear.
			// This way, memory doesn't get deallocated
			if (buf_.empty()) {
				using namespace std;
				swap(buf_, moved_buffer);
			}
			else {
				buf_.append(moved_buffer);
				moved_buffer.clear();
			}

			return *this;
		}

		template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>> * = nullptr>
		buffer_streamer &write(Enum e)
		{
			return write(std::underlying_type_t<Enum>(e));
		}

		buffer_streamer &write(const char s) { // <- const makes all the difference there. Removing it, will cause an infinite recursion.
			return write(&s, &s+1);
		}

		template <typename Integral, std::enable_if_t<std::is_integral_v<Integral>>* = nullptr>
		buffer_streamer &write(Integral i, std::size_t base = 10, unsigned char min_width = 0, unsigned char fill = ' ', unsigned char prefix = '\0') {
			util::convert(i, buf_, 1, base, min_width, fill, prefix);
			return *this;
		}

		template <typename... Ts>
		buffer_streamer &write(const std::tuple<Ts...> &t) {
			return write(t, std::make_index_sequence<sizeof... (Ts)>());
		}

		buffer_streamer &write(ref &r) {
			r = ref{buf_};
			return *this;
		}

		template <typename... Ts, std::size_t ...Is>
		buffer_streamer &write(const std::tuple<Ts...> &t, std::index_sequence<Is...>) {
			return (*this << ... << std::get<Is>(t));
		}

		buffer_streamer &write(fz::buffer &b) {
			buf_.append(b);
			b.clear();
			return *this;
		}

		buffer &buf_;
		std::function<void()> fin_;
	};

}

#endif // BUFFER_STREAMER_HPP
