#ifndef FZ_TRANSFORMED_VIEW_HPP
#define FZ_TRANSFORMED_VIEW_HPP

#include <iterator>
#include <array>

namespace fz {

template <typename Range, typename F>
class transformed_view
{
	using range_iterator = decltype(std::cbegin(std::declval<Range>()));
	using range_sentinel = decltype(std::cend(std::declval<Range>()));

public:
	using value_type = std::decay_t<decltype(std::declval<F>()(*std::declval<range_iterator>()))>;
	using size_type = std::size_t;

private:
	struct iterator;
	struct sentinel
	{
	private:
		friend transformed_view;
		friend iterator;

		constexpr sentinel(range_sentinel s)
			: s_(std::move(s))
		{}

		range_sentinel s_;
	};

	struct iterator
	{
		using iterator_category = std::input_iterator_tag;
		using difference_type = ptrdiff_t;
		using value_type = transformed_view::value_type;
		using pointer = value_type*;
		using reference = value_type&;

		constexpr bool operator !=(const sentinel &rhs) const {
			return it_ != rhs.s_;
		}

		constexpr bool operator ==(const sentinel &rhs) const {
			return !operator !=(rhs);
		}

		constexpr iterator &operator ++() {
			++it_;
			return *this;
		}

		constexpr iterator operator ++(int) {
			iterator copy(*this);
			operator++();
			return copy;
		}

		constexpr value_type operator *() const {
			return (*f_)(*it_);
		}

		struct value_type_proxy {
			constexpr value_type_proxy(value_type &&v):
				value_(std::move(v))
			{}

			constexpr const value_type *operator->() const {
				return &value_;
			}

			constexpr value_type *operator->() {
				return &value_;
			}

		private:
			value_type value_;
		};

		constexpr value_type_proxy operator->() const {
			return value_type_proxy(operator *());
		}

	private:
		friend transformed_view;

		constexpr iterator(range_iterator it, const F &f)
			: it_(std::move(it))
			, f_(&f)
		{}

		range_iterator it_;
		const std::remove_reference_t<F> *f_;
	};

	using const_iterator = iterator;

public:
	constexpr transformed_view(Range &&r, F &&f)
		: r_(std::forward<Range>(r))
		, f_(std::forward<F>(f))
	{}

	constexpr iterator begin() const noexcept
	{
		return {std::cbegin(r_), f_};
	}

	constexpr sentinel end() const noexcept
	{
		return {std::cend(r_)};
	}

private:
	Range r_;
	F f_;
};

template <typename Range, typename F>
transformed_view(Range &&r, F &&f) -> transformed_view<Range, F>;

template <typename T, std::size_t N, typename F>
transformed_view(T(&&v)[N], F &&f) -> transformed_view<std::array<T, N>, F>;

}

#endif // FZ_TRANSFORMED_VIEW_HPP
