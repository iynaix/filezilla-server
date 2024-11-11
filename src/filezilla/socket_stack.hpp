#ifndef FZ_SOCKET_STACK_HPP
#define FZ_SOCKET_STACK_HPP

#include <libfilezilla/socket.hpp>
#include <list>

namespace fz {

class socket_stack_interface: public socket_interface
{
public:
	using socket_interface::socket_interface;

	virtual void push(std::unique_ptr<socket_layer> socket_layer) = 0;
	virtual void pop() = 0;
	virtual socket_interface &top() const = 0;

	// parts of socket_interface we're interested in.
	virtual address_type address_family() const = 0;
	virtual std::string local_ip(bool strip_zone_index = false) const = 0;
	virtual int local_port(int& error) const = 0;
	virtual std::string peer_ip() const = 0;
	virtual int flags() const = 0;
	virtual void set_flags(int flags, bool enable) = 0;
	virtual void set_flags(int flags) = 0;
	virtual void set_keepalive_interval(duration const& d) = 0;
	virtual int set_buffer_sizes(int size_receive, int size_send) = 0;
	virtual bool bind(std::string const& address) = 0;

	// Utility
	template <typename Layer, typename... Args, typename std::enable_if_t<std::is_base_of_v<socket_layer, Layer>>* = nullptr>
	Layer &emplace(Args &&... args)
	{
		this->push(std::make_unique<Layer>(std::forward<Args>(args)...));
		return static_cast<Layer &>(this->top());
	}
};

class socket_stack: public socket_stack_interface
{
public:
	socket_stack(std::unique_ptr<socket> socket)
		: socket_stack_interface(socket->root())
		, socket_(*socket)
	{
		interfaces_.push_back(std::move(socket));
	}

	~socket_stack() override
	{
		// Destroy layers in reverse order
		while (!interfaces_.empty())
			interfaces_.pop_back();
	}

	void push(std::unique_ptr<socket_layer> socket_layer) override
	{
		interfaces_.push_back(std::move(socket_layer));
	}

	void pop() override
	{
		interfaces_.pop_back();
	}

	socket_interface &top() const override
	{
		return *interfaces_.back();
	}

	address_type address_family() const override
	{
		return socket_.address_family();
	}

	std::string local_ip(bool strip_zone_index = false) const override
	{
		return socket_.local_ip(strip_zone_index);
	}

	int local_port(int& error) const override
	{
		return socket_.local_port(error);
	}

	std::string peer_ip() const override
	{
		return socket_.peer_ip();
	}

	int flags() const override
	{
		return socket_.flags();
	}

	void set_flags(int flags, bool enable) override
	{
		socket_.set_flags(flags, enable);
	}

	void set_flags(int flags) override
	{
		socket_.set_flags(flags);
	}

	void set_keepalive_interval(duration const& d) override
	{
		socket_.set_keepalive_interval(d);
	}

	int set_buffer_sizes(int size_receive, int size_send) override
	{
		return socket_.set_buffer_sizes(size_receive, size_send);
	}

	bool bind(std::string const& address) override
	{
		return socket_.bind(address);
	}

	int read(void *buffer, unsigned int size, int &error) override
	{
		return top().read(buffer, size, error);
	}

	int write(const void *buffer, unsigned int size, int &error) override
	{
		return top().write(buffer, size, error);
	}

	void set_event_handler(event_handler *pEvtHandler, fz::socket_event_flag retrigger_block = {}) override
	{
		return top().set_event_handler(pEvtHandler, retrigger_block);
	}

	native_string peer_host() const override
	{
		return top().peer_host();
	}

	int peer_port(int &error) const override
	{
		return top().peer_port(error);
	}

	int connect(const native_string &host, unsigned int port, address_type family = address_type::unknown) override
	{
		return top().connect(host, port, family);
	}

	socket_state get_state() const override
	{
		return top().get_state();
	}

	int shutdown() override
	{
		return top().shutdown();
	}

	int shutdown_read() override
	{
		return top().shutdown_read();
	}

private:
	fz::socket &socket_;
	std::list<std::unique_ptr<socket_interface>> interfaces_{};
};


}
#endif // FZ_SOCKET_STACK_HPP
