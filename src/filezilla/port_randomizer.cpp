#include "port_randomizer.hpp"

#include <random>

namespace fz {

port_randomizer::port_randomizer(port_manager & manager, std::string const& peer_ip, int min_port, int max_port)
	: min_(min_port)
	, max_(max_port)
	, peer_ip_(peer_ip)
	, manager_(manager)
{
	if (min_ > max_) {
		std::swap(min_, max_);
	}
	if (min_ <= 0) {
		min_ = 1;
	}
	else if (min_ > 65535) {
		min_ = 65535;
	}
	if (max_ <= 0) {
		max_ = 1;
	}
	else if (max_ > 65535) {
		max_ = 65535;
	}

	// Start with a random port in the range
	std::random_device rd;
	first_port_ = std::uniform_int_distribution<int>(min_, max_)(rd);
}

port_lease port_randomizer::get_port()
{
	return port_lease(do_get_port(), peer_ip_, manager_);
}

int port_randomizer::do_get_port()
{
	auto now = fz::monotonic_clock::now();

	while (true) {
		if (!prev_port_) {
			prev_port_ = first_port_;
		}
		else {
			++prev_port_;
			if (prev_port_ < min_ || prev_port_ > max_) {
				prev_port_ = min_;
			}
			if (prev_port_ == first_port_) {
				// Wraparound, relax requirements
				if (!allow_reuse_other_) {
					// Should not be a problem other than when using server-to-server transfers
					allow_reuse_other_ = true;
				}
				else if (!allow_reuse_same_) {
					// This can be problematic in case peer port is the same due to the socket pair's TIME_WAIT state.
					allow_reuse_same_ = true;
				}
				else {
					// Give up
					break;
				}
			}
		}

		fz::scoped_lock l(manager_.mutex_[prev_port_]);

		if (!manager_.connecting_[prev_port_].exchange(1)) {
			if (!allow_reuse_other_ && !allow_reuse_same_) {
				manager_.prune(prev_port_, now);
			}

			auto& es = manager_.entries_[prev_port_];
			auto it = std::find_if(es.begin(), es.end(), [&](port_manager::entry const& e){ return e.peer_ == peer_ip_; });
			if (it != es.end()) {
				if (allow_reuse_same_) {
					++it->leases_;
					return prev_port_;
				}
			}
			else if (es.empty() || allow_reuse_other_) {
				port_manager::entry e;
				e.leases_ = 1;
				e.peer_ = peer_ip_;
				es.push_back(e);
				return prev_port_;
			}
			manager_.connecting_[prev_port_] = 0;
		}
	}

	return 0;
}


void port_manager::release(int p, std::string const& peer, bool connected)
{
	if (p && p < 65536) {
		{
			fz::scoped_lock l(mutex_[p]);

			auto& es = entries_[p];
			auto it = std::find_if(es.begin(), es.end(), [&](entry const& e) { return e.peer_ == peer; });
			if (it != es.end() && it->leases_) {
				--it->leases_;
				it->expiry_ = fz::monotonic_clock::now() + fz::duration::from_minutes(4); // 4 minute TIME_WAIT
			}
		}

		if (!connected) {
			connecting_[p] = 0;
		}
	}
}


void port_manager::set_connected(int p, const std::string &)
{
	if (p && p < 65536) {
		connecting_[p] = 0;
	}
}


void port_manager::prune(int port, const fz::monotonic_clock &now)
{
	auto const predicate = [&now](entry const& e) {
		return !e.leases_ && e.expiry_ < now;
	};

	entries_[port].erase(
		std::remove_if(entries_[port].begin(), entries_[port].end(), predicate),
		entries_[port].end()
	);
}


port_lease::port_lease(port_lease && lease)
	: port_(lease.port_)
	, peer_ip_(std::move(lease.peer_ip_))
	, port_manager_(lease.port_manager_)
	, connected_(lease.connected_)
{
	lease.port_ = 0;
}

port_lease& port_lease::operator=(port_lease && lease)
{
	if (port_manager_)
		port_manager_->release(port_, peer_ip_, connected_);

	port_ = lease.port_;
	peer_ip_ = std::move(lease.peer_ip_);
	port_manager_ = lease.port_manager_;
	connected_ = lease.connected_;
	lease.port_ = 0;
	return *this;
}

port_lease::~port_lease()
{
	if (port_manager_)
		port_manager_->release(port_, peer_ip_, connected_);
}

port_lease::port_lease(int p, std::string const& peer, port_manager & manager)
	: port_(p)
	, peer_ip_(peer)
	, port_manager_(&manager)
{
}

void port_lease::set_connected()
{
	if (port_manager_ && !connected_) {
		connected_ = true;
		port_manager_->set_connected(port_, peer_ip_);
	}
}

fz::mutex port_manager::mutex_[65536];
std::vector<port_manager::entry> port_manager::entries_[65536];
std::atomic_char port_manager::connecting_[65536];

}
