#include "archives.hpp"

namespace fz::impersonator {

std::unordered_set<serialization::binary_output_archive*> output_archive::arset_;
std::unordered_set<serialization::binary_input_archive*> input_archive::arset_;

}

namespace fz::serialization {

void load(impersonator::input_archive &ar, tvfs::fd_owner &fd)
{
	fd.reset();

	if (bool has_fd = false; ar(has_fd) && has_fd) {
		if (ar.fds_buf_.empty()) {
			ar.error(EINVAL);
			return;
		}

		fd = std::move(ar.fds_buf_.front());
		ar.fds_buf_.pop_front();
	}
}

void save(impersonator::output_archive &ar, tvfs::fd_owner &fd)
{
	if (ar.out_fd_) {
		// We only allow one fd to be present in each serialization;
		ar.error(EEXIST);
		return;
	}

	ar.out_fd_ = std::move(fd);
	ar(ar.out_fd_.is_valid());
}

void load(serialization::binary_input_archive &ar, tvfs::fd_owner &fd)
{
	if (auto it = impersonator::input_archive::arset_.find(&ar); it != impersonator::input_archive::arset_.end())
		load(static_cast<impersonator::input_archive&>(ar), fd);
	else
		ar.error(EINVAL);
}

void save(serialization::binary_output_archive &ar, tvfs::fd_owner &fd)
{
	if (auto it = impersonator::output_archive::arset_.find(&ar); it != impersonator::output_archive::arset_.end())
		save(static_cast<impersonator::output_archive&>(ar), fd);
	else
		ar.error(EINVAL);
}

}
