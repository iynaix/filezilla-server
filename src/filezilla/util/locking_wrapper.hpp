#ifndef FZ_UTIL_LOCKING_WRAPPER_HPP
#define FZ_UTIL_LOCKING_WRAPPER_HPP

#include <utility>

#include <libfilezilla/mutex.hpp>
#include "../util/traits.hpp"

namespace fz::util {

template <typename T, typename Mutex = fz::mutex>
class locking_wrapper;

template <typename T, typename MutexPtr = fz::mutex*, typename SFINAE = void>
class locked_proxy
{
public:
	using value_type = T;
	using mutex_ptr_type = MutexPtr;

	locked_proxy(T *value = nullptr, MutexPtr mutex = nullptr)
		: value_(value)
		, mutex_(mutex)
	{
	}

	locked_proxy(locked_proxy &&rhs) noexcept
		: locked_proxy()
	{
		using std::swap;

		swap(*this, rhs);
	}

	locked_proxy &operator=(locked_proxy &&rhs) noexcept
	{
		locked_proxy copy(std::move(rhs));

		using std::swap;

		swap(*this, copy);

		return *this;
	}

	locked_proxy(const locked_proxy &) = delete;
	locked_proxy operator =(const locked_proxy &) = delete;

	~locked_proxy()
	{
		if (mutex_)
			mutex_->unlock();
	}

	T &operator *()
	{
		return *value_;
	}

	T *operator ->()
	{
		return value_;
	}

	const T *operator ->() const
	{
		return value_;
	}

	explicit operator bool() const
	{
		return value_ != nullptr;
	}

	T *get()
	{
		return value_;
	}

	friend void swap(locked_proxy &lhs, locked_proxy &rhs) noexcept
	{
		using std::swap;

		swap(lhs.mutex_, rhs.mutex_);
		swap(lhs.value_, rhs.value_);
	}

	template <typename A, std::enable_if_t<std::is_convertible_v<T*, A*>> = nullptr>
	operator locked_proxy<A, MutexPtr>() &&
	{
		locked_proxy<A, MutexPtr> ret {value_, mutex_};

		mutex_ = nullptr;
		value_ = nullptr;

		return ret;
	}

	template <typename A, typename B, typename M>
	friend locked_proxy<A, M> static_locked_proxy_cast(locked_proxy<B, M> &&b);

	template <typename A, typename B, typename M>
	friend locked_proxy<A, M> dynamic_locked_proxy_cast(locked_proxy<B, M> &&b);

	template <typename A, typename B, typename M>
	friend locked_proxy<A, M> reintepret_locked_proxy_cast(locked_proxy<B, M> &&b);

private:
	T *value_;
	MutexPtr mutex_;
};

FZ_UTIL_TRAITS_MAKE_ACCESS_TEMPLATE(locked_proxy)

template <typename A, typename B, typename MutexPtr>
locked_proxy<A, MutexPtr> static_locked_proxy_cast(locked_proxy<B, MutexPtr> &&b)
{
	locked_proxy<A, MutexPtr> ret {static_cast<A*>(b.value_), b.mutex_};

	b.value_ = nullptr;
	b.mutex_ = nullptr;

	return ret;
}

template <typename A, typename B, typename MutexPtr>
locked_proxy<A, MutexPtr> dynamic_locked_proxy_cast(locked_proxy<B, MutexPtr> &&b)
{
	if (A *casted = dynamic_cast<A*>(b.value_)) {
		locked_proxy<A, MutexPtr> ret {casted, b.mutex_};

		b.value_ = nullptr;
		b.mutex_ = nullptr;

		return ret;
	}

	return {};
}

template <typename A, typename B, typename MutexPtr>
locked_proxy<A, MutexPtr> reintepret_locked_proxy_cast(locked_proxy<B, MutexPtr> &&b)
{
	locked_proxy<A, MutexPtr> ret {reinterpret_cast<A*>(b.value_), b.mutex_};

	b.value_ = nullptr;
	b.mutex_ = nullptr;

	return ret;
}

template <typename Proxy>
class locked_proxy<Proxy, typename Proxy::mutex_ptr_type, std::enable_if_t<trait::is_locked_proxy_v<Proxy>>>
{
public:
	using value_type = typename Proxy::value_type;
	using mutex_ptr_type = typename Proxy::mutex_ptr_type;

	locked_proxy()
		: proxy_()
		, mutex_(nullptr)
	{}

	locked_proxy(Proxy &&proxy, mutex_ptr_type mutex)
		: proxy_(std::move(proxy))
		, mutex_(mutex)

	{}

	locked_proxy(locked_proxy &&rhs) noexcept
		: locked_proxy()
	{
		using std::swap;

		swap(*this, rhs);
	}

	locked_proxy &operator=(locked_proxy &&rhs) noexcept
	{
		locked_proxy copy(std::move(rhs));

		using std::swap;

		swap(*this, copy);

		return *this;
	}

	locked_proxy(const locked_proxy &) = delete;
	locked_proxy operator =(const locked_proxy &) = delete;

	~locked_proxy()
	{
		if (mutex_)
			mutex_->unlock();
	}

	value_type &operator *()
	{
		return *proxy_;
	}

	value_type *operator ->()
	{
		return &*proxy_;
	}

	explicit operator bool() const
	{
		return bool(proxy_);
	}

	value_type *get()
	{
		return proxy_.get();
	}

	friend void swap(locked_proxy &lhs, locked_proxy &rhs) noexcept
	{
		using std::swap;

		swap(lhs.mutex_, rhs.mutex_);
		swap(lhs.proxy_, rhs.proxy_);
	}

private:
	Proxy proxy_;
	mutex_ptr_type mutex_;
};

template <typename T>
class locking_wrapper_interface
{
public:
	using proxy = locked_proxy<T>;

	virtual ~locking_wrapper_interface() = default;
	virtual proxy lock() = 0;
};

template <typename T>
class locking_wrapper<T, fz::mutex>: public locking_wrapper_interface<T>
{
public:
	template <typename ...Args>
	locking_wrapper(Args &&... args)
		: value_(std::forward<Args>(args)...)
	{}

	template <typename ...Args>
	locking_wrapper(std::in_place_t, Args &&... args)
		: value_{std::forward<Args>(args)...}
	{}

	locked_proxy<T> lock() override
	{
		mutex_.lock();

		return {&value_, &mutex_};
	}

private:
	T value_;
	fz::mutex mutex_{true};
};

template <typename T>
class locking_wrapper<T, fz::mutex&>: public locking_wrapper_interface<T>
{
public:
	template <typename ...Args>
	locking_wrapper(fz::mutex &mutex, Args &&... args)
		: mutex_(mutex)
		, value_(std::forward<Args>(args)...)
	{}

	template <typename ...Args>
	locking_wrapper(fz::mutex &mutex, std::in_place_t, Args &&... args)
		: mutex_(mutex)
		, value_{std::forward<Args>(args)...}
	{}

	locked_proxy<T> lock() override
	{
		mutex_.lock();

		return {&value_, &mutex_};
	}

private:
	fz::mutex &mutex_;
	T value_;
};

template <typename T>
class locking_wrapper<T, void>: public locking_wrapper_interface<T>
{
public:
	template <typename ...Args>
	locking_wrapper(Args &&... args)
		: value_(std::forward<Args>(args)...)
	{}

	template <typename ...Args>
	locking_wrapper(std::in_place_t, Args &&... args)
		: value_{std::forward<Args>(args)...}
	{}

	locked_proxy<T> lock() override
	{
		return {&value_, nullptr};
	}

private:
	T value_;
};

template <typename T>
class locking_wrapper<T&, fz::mutex>: public locking_wrapper_interface<T>
{
public:
	locking_wrapper(T &value)
		: value_(value)
	{}

	locked_proxy<T> lock() override
	{
		mutex_.lock();

		return {&value_, &mutex_};
	}

private:
	fz::mutex mutex_{true};
	T &value_;
};

template <typename T>
class locking_wrapper<T&, fz::mutex&>: public locking_wrapper_interface<T>
{
public:
	locking_wrapper(fz::mutex &mutex, T &value)
		: mutex_(mutex)
		, value_(value)
	{}

	locked_proxy<T> lock() override
	{
		mutex_.lock();

		return {&value_, &mutex_};
	}

private:
	fz::mutex &mutex_;
	T &value_;
};

template <typename T>
class locking_wrapper<T&, void>: public locking_wrapper_interface<T>
{
public:
	locking_wrapper(T &value)
		: value_(value)
	{}

	locked_proxy<T> lock() override
	{
		return {&value_, nullptr};
	}

private:
	T &value_;
};

}

#endif // FZ_UTIL_LOCKING_WRAPPER_HPP
