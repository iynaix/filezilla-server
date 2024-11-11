#ifndef FZ_SHARED_CONTEXT_HPP
#define FZ_SHARED_CONTEXT_HPP

#include <cassert>

#include <memory>

#include "util/locking_wrapper.hpp"
#include "util/traits.hpp"

namespace fz {

namespace detail {

struct shared_context_data_do_not_delete
{
	template <typename T>
	void operator()(T *){}
};

}

template <typename T>
struct shared_context_data_impl;

template <typename T, typename Mutex = fz::mutex>
struct shared_context_data;

template <typename T, typename Mutex = fz::mutex>
struct shared_context;

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(shared_context)

template <typename T>
struct shared_context_data_impl<T *>
{
	using value_type = T;

	shared_context_data_impl(T *v)
		: v_(v)
	{}

	std::unique_ptr<T, detail::shared_context_data_do_not_delete> v_{};
};

template <typename T>
struct shared_context_data_impl<std::unique_ptr<T>>
{
	using value_type = T;

	shared_context_data_impl(std::unique_ptr<T> v)
		: v_(std::move(v))
	{}

	std::unique_ptr<T> v_;
};


template <typename T>
struct shared_context_data_impl<std::optional<T>>
{
	using value_type = T;

	template <typename U>
	shared_context_data_impl(U && v)
		: v_(std::forward<U>(v))
	{}

	std::optional<T> v_;
};

template <typename T>
struct shared_context_data_impl
{
	using value_type = T;

	template <typename U>
	shared_context_data_impl(U && v)
		: v_(std::forward<U>(v))
	{}

	std::optional<T> v_;
};

template <typename T>
struct shared_context_data<T, fz::mutex>: shared_context_data_impl<T>
{
	using value_type = typename shared_context_data_impl<T>::value_type;

	template <typename U>
	shared_context_data(U && v)
		: shared_context_data_impl<T>(std::forward<U>(v))
	{}

	util::locked_proxy<const value_type> lock() const
	{
		mutex_.lock();

		if (this->v_) {
			return {&*this->v_, &mutex_};
		}

		mutex_.unlock();

		return { nullptr, nullptr };
	}

	util::locked_proxy<value_type> lock()
	{
		mutex_.lock();

		if (this->v_) {
			return {&*this->v_, &mutex_};
		}

		mutex_.unlock();

		return { nullptr, nullptr };
	}

	explicit operator bool() const noexcept
	{
		scoped_lock lock(mutex_);

		return bool(this->v_);
	}

	void stop_sharing()
	{
		scoped_lock lock(mutex_);
		this->v_.reset();
	}

	mutable fz::mutex mutex_;
};

template <typename T, typename Mutex>
struct shared_context final
{
	using data_type = shared_context_data<T, Mutex>;
	using value_type = typename data_type::value_type;

	shared_context() = default;

	shared_context(shared_context &&) = default;
	shared_context(const shared_context &) = default;

	shared_context &operator=(shared_context &&) = default;
	shared_context &operator=(const shared_context &) = default;

	template <typename U, typename = std::enable_if_t<!trait::is_a_shared_context_v<U>>>
	shared_context(U && v)
		: data_(std::make_shared<shared_context_data<T, Mutex>>(std::forward<U>(v)))
	{}

	util::locked_proxy<const value_type> lock() const
	{
		if (data_) {
			return data_->lock();
		}

		return { nullptr, nullptr };
	}


	util::locked_proxy<value_type> lock()
	{
		if (data_) {
			return data_->lock();
		}

		return { nullptr, nullptr };
	}

	explicit operator bool() const noexcept
	{
		if (data_) {
			return bool(*data_);
		}

		return false;
	}

	void detach() noexcept
	{
		data_.reset();
	}

	void stop_sharing() noexcept {
		if (data_) {
			data_->stop_sharing();
		}
	}

private:
	std::shared_ptr<shared_context_data<T, Mutex>> data_;
};


}

#endif // FZ_SHARED_CONTEXT_HPP
