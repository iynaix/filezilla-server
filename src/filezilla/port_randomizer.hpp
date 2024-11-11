#ifndef FZ_PORT_RANDOMIZER_HPP
#define FZ_PORT_RANDOMIZER_HPP

// Ported almost verbatim from old filezilla server's sources.

#include <atomic>
#include <string>

#include <libfilezilla/mutex.hpp>

/*
FTP suffers from connection stealing attacks. The only actual solution
to this problem that prevents all attacks is TLS session resumption.

To mitigate (but not eliminate) this problem when using plaintext sessions,
the passive mode port should at least be randomized.

The randomizer picks a random free port from the assigned passive mode range.

If there is no free port, it picks a used port with a different peer, provided
said port is not in the listening stage.

As last resort, it reuses a busy port from the same peer.

*/

namespace fz {

class port_manager;
class port_lease final
{
public:
	port_lease() = default;
	~port_lease();

	port_lease(port_lease && lease);
	port_lease& operator=(port_lease && lease);

	port_lease(port_lease const&) = delete;
	port_lease& operator=(port_lease const&) = delete;

	int get_port() const { return port_; }
	void set_connected();

private:
	friend class port_randomizer;
	friend class port_manager;

	port_lease(int p, std::string const& peer, port_manager & manager);

	int port_{};
	std::string peer_ip_;
	port_manager * port_manager_{};
	bool connected_{};
};

class port_randomizer final
{
public:
	enum : std::uint16_t {
		min_ephemeral_value = 49152,
		max_ephemeral_value = 65534
	};

	explicit port_randomizer(port_manager & manager, std::string const& peer_ip, int min_port = min_ephemeral_value, int max_port = max_ephemeral_value);

	port_randomizer(port_randomizer const&) = delete;
	port_randomizer& operator=(port_randomizer const&) = delete;

	port_lease get_port();

private:
	int do_get_port();

	int min_{};
	int max_{};

	int first_port_{};
	int prev_port_{};

	bool allow_reuse_other_{};
	bool allow_reuse_same_{};

	std::string const peer_ip_;

	port_manager& manager_;
};


class port_manager final
{
public:
	port_manager() = default;
	port_manager(port_manager const&) = delete;
	port_manager& operator=(port_manager const&) = delete;

private:
	friend class port_lease;
	friend class port_randomizer;

	void release(int p, const std::string &peer, bool connected);
	void set_connected(int p, std::string const&);

	void prune(int port, const fz::monotonic_clock &now);

	struct entry
	{
		std::string peer_;
		int leases_{};
		fz::monotonic_clock expiry_{};
	};

	static fz::mutex mutex_[65536];
	static std::vector<entry> entries_[65536];
	static std::atomic_char connecting_[65536];
};

}

#endif // FZ_PORT_RANDOMIZER_HPP
