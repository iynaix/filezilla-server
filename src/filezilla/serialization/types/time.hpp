#ifndef FZ_SERIALIZATION_TYPES_DATETIME_HPP
#define FZ_SERIALIZATION_TYPES_DATETIME_HPP

#include <libfilezilla/time.hpp>

#include "../../serialization/serialization.hpp"

namespace fz::serialization {

	template <typename Archive>
	std::int64_t save_minimal(const Archive &, const datetime &dt)
	{
		return (dt-fz::datetime(0, datetime::milliseconds)).get_milliseconds();
	}

	template <typename Archive>
	void load_minimal(const Archive &, datetime &dt, std::int64_t millis_since_epoch)
	{
		dt = fz::datetime(0, datetime::milliseconds);
		dt += fz::duration::from_milliseconds(millis_since_epoch);
	}

	template <typename Archive>
	std::int64_t save_minimal(const Archive &, const duration &d)
	{
		return std::int64_t(d.get_milliseconds());
	}

	template <typename Archive>
	void load_minimal(const Archive &, duration &d, std::int64_t milli)
	{
		d = duration::from_milliseconds(milli);
	}

}

#endif // BUFFER_SERIALIZATION_DATETIME_HPP
