#ifndef FZ_UTIL_COPIES_COUNTER_H
#define FZ_UTIL_COPIES_COUNTER_H

#include <memory>
#include <string>

namespace fz::util
{

/// \brief A class whose instances count how many copies of themselves are alive.
class copies_counter
{
public:
	static constexpr std::size_t object_has_been_moved_from = static_cast<std::size_t>(-1);

	copies_counter() noexcept
		: copies_counter("")
	{}

	explicit copies_counter(std::string_view name)
		: name_(name.empty() ? nullptr : new std::string(name))
	{
	}

	std::size_t count() const noexcept
	{
		// use_count can be 0 only if the object has been moved from.
		return static_cast<std::size_t>(name_.use_count() - 1);
	}

	std::string_view name() const noexcept {
		return name_ ? *name_ : std::string_view();
	}

private:
	std::shared_ptr<std::string> name_;
};

/// \brief A class whose instances count how many copies of themselves are alive and reports whether a given count limit has been reached
class limited_copies_counter: public copies_counter
{
public:
	using copies_counter::copies_counter;

	void set_limit(std::size_t limit) noexcept
	{
		limit_ = limit;
	}

	bool limit_reached() const noexcept
	{
		return limit_ && limit_ <= count();
	}

private:
	std::size_t limit_{};
};

}
#endif // FZ_UTIL_COPIES_COUNTER_H
