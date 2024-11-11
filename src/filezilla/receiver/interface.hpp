#ifndef FZ_RECEIVER_INTERFACE_HPP
#define FZ_RECEIVER_INTERFACE_HPP

#include <tuple>

#include "context.hpp"

namespace fz {

struct receiver_interface
{
	virtual ~receiver_interface() = default;
	virtual void receive_in(receiver_context &rc) = 0;
	virtual void done_in(receiver_context &rc) = 0;
	virtual std::size_t event_type() const = 0;
};

template <typename... Values>
struct valued_receiver_interface: receiver_interface
{
	using tuple_type = std::tuple<Values...>;

	~valued_receiver_interface() override
	{
		if (!engaged_)
			return;

		v_.~tuple_type();
	}

	union {
		char dummy_;
		mutable tuple_type v_;
	};

	template <typename... Ts, std::enable_if_t<sizeof...(Ts) == sizeof...(Values) && std::is_constructible_v<tuple_type, Ts...>>* = nullptr>
	void receive_in(receiver_context &rc, Ts &&... vs) noexcept(noexcept(tuple_type(std::forward<Ts>(vs)...)))
	{
		if (engaged_)
			v_.~tuple_type();

		new (&v_) tuple_type(std::forward<Ts>(vs)...);
		engaged_ = true;

		receive_in(rc);
	}

protected:
	valued_receiver_interface(){}

private:
	using receiver_interface::receive_in;

	bool engaged_{};
};


template <>
struct valued_receiver_interface<>: receiver_interface
{
	using tuple_type = std::tuple<>;

	mutable tuple_type v_;

protected:
	valued_receiver_interface() = default;
};


}

#endif // FZ_RECEIVER_INTERFACE_HPP
