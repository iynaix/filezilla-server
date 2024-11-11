#ifndef FZ_IMPERSONATOR_ARCHIVES_HPP
#define FZ_IMPERSONATOR_ARCHIVES_HPP

#include <deque>
#include <unordered_set>

#include <libfilezilla/file.hpp>
#include "../debug.hpp"

#include "../serialization/archives/binary.hpp"
#include "../logger/modularized.hpp"
#include "../tvfs/backend.hpp"

namespace fz {

namespace impersonator {

	class output_archive;
	class input_archive;

}

namespace serialization {

	void save(serialization::binary_output_archive &ar, tvfs::fd_owner &fd);
	void save(impersonator::output_archive &ar, tvfs::fd_owner &fd);

	void load(serialization::binary_input_archive &ar, tvfs::fd_owner &fd);
	void load(impersonator::input_archive &ar, tvfs::fd_owner &fd);

}

}

namespace fz::impersonator {

class output_archive;
class input_archive;

using tvfs::fd_owner;
using tvfs::fd_t;
using tvfs::invalid_fd;
using tvfs::is_valid_fd;

class output_archive: public serialization::binary_output_archive
{
public:
	output_archive(buffer &buf, fd_owner &out_fd)
		: binary_output_archive(buf)
		, out_fd_(out_fd)
	{
		out_fd = {};
		arset_.emplace(this);
	}

	~output_archive()
	{
		arset_.erase(this);
	}

private:
	logger::modularized logger_;

	fd_owner &out_fd_;

	static std::unordered_set<serialization::binary_output_archive*> arset_;

	friend void serialization::save(serialization::binary_output_archive &ar, tvfs::fd_owner &fd);
	friend void serialization::save(output_archive &ar, tvfs::fd_owner &fd);
};

class input_archive: public serialization::binary_input_archive
{
public:
	static constexpr auto max_payload_size = serialization::size_type(64*1024);

	input_archive(buffer &buf, std::deque<fd_owner> &fds_buf)
		: binary_input_archive(buf, max_payload_size)
		, fds_buf_(fds_buf)
	{
		arset_.emplace(this);
	}

	~input_archive()
	{
		arset_.erase(this);
	}

private:
	std::deque<fd_owner> &fds_buf_;

	static std::unordered_set<serialization::binary_input_archive*> arset_;

	friend void serialization::load(serialization::binary_input_archive &ar, tvfs::fd_owner &fd);
	friend void serialization::load(input_archive &ar, tvfs::fd_owner &fd);
};

}

FZ_SERIALIZATION_LINK_ARCHIVES(fz::impersonator::input_archive, fz::impersonator::output_archive)

#endif // FZ_IMPERSONATOR_ARCHIVES_HPP
