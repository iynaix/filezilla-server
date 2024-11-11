#ifndef FZ_UTIL_WELCOME_MESSAGE_HPP
#define FZ_UTIL_WELCOME_MESSAGE_HPP

#include <string>

namespace fz::util {

struct welcome_message: std::string
{
	welcome_message()
		: std::string()
		, has_version(true)
	{}

	welcome_message(std::string message, bool has_version = true)
		: std::string(std::move(message))
		, has_version(has_version)
	{}

	struct validate_result
	{
		static constexpr std::size_t line_limit = 1024;
		static constexpr std::size_t total_limit = 100*1024;

		enum which
		{
			ok                 = 0,
			total_size_too_big = 1,
			line_too_long      = 2,
		};

		validate_result(which result, std::string_view data)
			: result_(result)
			, data_(data)
		{
		}

		explicit operator bool() const
		{
			return result_ == ok;
		}

		operator which() const
		{
			return result_;
		}

		std::string_view data() const
		{
			return data_;
		}

	private:
		which result_;
		std::string_view data_;
	};

	validate_result validate() const;

	bool has_version;
};

}

#endif // FZ_UTIL_WELCOME_MESSAGE_HPP
