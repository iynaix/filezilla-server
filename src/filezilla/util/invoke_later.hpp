#ifndef FZ_UTIL_INVOKE_LATER_HPP
#define FZ_UTIL_INVOKE_LATER_HPP

#include <libfilezilla/event_handler.hpp>

#include "../util/traits.hpp"

namespace fz::util {


/// \brief
/// Invokes the callable Func, which must take no parameters, only when its return value is actually needed.
/// Func must have only one operator()() and must return anything but void.
template <typename Func, typename R = std::invoke_result_t<Func>, std::enable_if_t<is_uniquely_invocable_v<Func> && !std::is_same_v<void,R>>* = nullptr>
auto invoke_later(Func && func)
{
	struct invoker
	{
		invoker(Func && func)
			: func_(std::forward<Func>(func))
		{
		}

		operator R()
		{
			return func_();
		}

		Func func_;
	};

	return invoker(std::forward<Func>(func));
}

/// \brief
/// Invokes the callable Func, with the given Arg, Args..., only when its return value is actually needed.
template <typename Func, typename Arg, typename... Args>
auto invoke_later(Func && func, Arg && arg, Args &&... args) -> decltype(std::invoke(std::forward<Func>(func), std::forward<Arg>(arg), std::forward<Args>(args)...))
{
	return invoke_later([func = std::forward<Func>(func), targs = std::tuple<Arg, Args...>(std::forward<Arg>(arg), std::forward<Args>(args)...)]() mutable -> decltype(auto) {
		return std::apply([&func](auto &&... args) -> decltype(auto) {
			return std::invoke(func, std::forward<decltype(args)>(args)...);
		}, std::move(targs));
	});
}

class invoker_handler;

class invoker_event: public event_base
{
public:
	virtual void operator()() const = 0;

	static void invoke(invoker_event &ie)
	{
		ie();
	}

	static size_t type() {
		static size_t const v = get_unique_type_id(typeid(invoker_event));
		return v;
	}

	size_t derived_type() const override {
		return type();
	}

	std::tuple<invoker_event &> v_ { *this };
};

/// \brief
/// Invokes the callable Func, which must take no parameters, in the context of the event_loop associated with the given event_handler.
/// Func must have only one operator()()
template <typename Func, std::enable_if_t<is_uniquely_invocable_v<Func>>* = nullptr>
void invoke_later(event_handler *eh, Func func)
{
	struct event: invoker_event
	{
		event(Func func)
			: func_(std::move(func))
		{
		}

		void operator()() const override
		{
			const_cast<event *>(this)->func_();
		}

		Func func_;
	};

	eh->send_event<event>(std::move(func));
}

/// \brief
/// Invokes the callable Func, with the given Arg, Args...,  in the context of the event_loop associated with the given event_handler.
template <typename Func, typename Arg, typename... Args>
auto invoke_later(event_handler *eh, Func && func, Arg && arg, Args &&... args) -> decltype(std::invoke(func, std::forward<Arg>(arg), std::forward<Args>(args)...), void())
{
	invoke_later(eh, [func = std::forward<Func>(func), targs = std::tuple<Arg, Args...>(std::forward<Arg>(arg), std::forward<Args>(args)...)]() mutable {
		std::apply([&func](auto &&... args) {
			std::invoke(func, std::forward<decltype(args)>(args)...);
		}, targs);
	});
}

/// \brief
/// Invokes the callable Func, with the given Args..., in the context of the event_loop associated with the given event_handler.
template <typename Func, typename... Args>
auto invoke_later(event_handler &eh, Func && func, Args &&... args) -> decltype(invoke_later(&eh, std::forward<Func>(func), std::forward<Args>(args)...))
{
	return invoke_later(&eh, std::forward<Func>(func), std::forward<Args>(args)...);
}

/// \brief
/// \returns an invoker of Func. Func will be invoked in the context of the given event_loop with the parameters used at the time of the call.
template <typename Func>
auto make_invoker(event_loop &loop, Func && func)
{
	return [eh = std::make_shared<invoker_handler>(loop), func = std::forward<Func>(func)](auto &&... args) -> decltype(invoke_later(std::declval<event_handler&>(), std::cref(func), std::forward<decltype(args)>(args)...))
	{
		return invoke_later(*eh, std::cref(func), std::forward<decltype(args)>(args)...);
	};
}


/// \brief
/// \returns an invoker of Func. Func will be invoked in the context of the given event_loop with the parameters Arg, Args... and then any additional parameters used at the time of the call.
template <typename Func, typename Arg, typename... Args>
auto make_invoker(event_loop &loop, Func && func, Arg && arg, Args &&... args)
{
	return make_invoker(loop,
		[
		   func = std::forward<Func>(func),
		   targs = std::tuple<Arg, Args...>(std::forward<Arg>(arg), std::forward<Args>(args)...)
		] (auto &&... additional_args) -> decltype(std::invoke(func, std::forward<Arg>(arg), std::forward<Args>(args)..., std::forward<decltype(additional_args)>(additional_args)...))
		{
			std::apply([&func, &additional_args...](auto &&... args) {
				return std::invoke(func, std::forward<decltype(args)>(args)..., std::forward<decltype(additional_args)>(additional_args)...);
			}, targs);
		}
	);
}


/// \brief
/// \returns an invoker of Func. Func will be invoked in the context of the event_loop associated with the given event_handler with the parameters Arg, Args... and then any additional parameters used at the time of the call.
template <typename Func, typename... Args>
auto make_invoker(event_handler &eh, Func && func, Args &&... args)
{
	return make_invoker(eh.event_loop_, std::forward<Func>(func), std::forward<Args>(args)...);
}

/// \brief
/// \returns an invoker of Func. Func will be invoked in the context of the event_loop associated with the given event_handler with the parameters Arg, Args... and then any additional parameters used at the time of the call.
template <typename Func, typename... Args>
auto make_invoker(event_handler *eh, Func && func, Args &&... args)
{
	return make_invoker(eh->event_loop_, std::forward<Func>(func), std::forward<Args>(args)...);
}

class invoker_handler: public event_handler
{
public:
	invoker_handler(event_loop &loop);
	~invoker_handler() override;

	template <typename Func, typename... Args>
	auto operator()(Func && func, Args &&... args) -> decltype(fz::util::invoke_later(this, std::forward<Func>(func), std::forward<Args>(args)...))
	{
		return fz::util::invoke_later(this, std::forward<Func>(func), std::forward<Args>(args)...);
	}

	template <typename Func, typename... Args>
	auto invoke_later(Func && func, Args &&... args) -> decltype(fz::util::invoke_later(this, std::forward<Func>(func), std::forward<Args>(args)...))
	{
		return fz::util::invoke_later(this, std::forward<Func>(func), std::forward<Args>(args)...);
	}

protected:
	bool on_invoker_event(const event_base &ev)
	{
		return dispatch<invoker_event>(ev, invoker_event::invoke);
	}

private:
	void operator()(const event_base &ev) override
	{
		dispatch<invoker_event>(ev, invoker_event::invoke);
	}
};

}

#endif // FZ_UTIL_INVOKE_LATER_HPP
