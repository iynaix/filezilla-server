#ifndef FZ_UPDATE_INFO_HPP
#define FZ_UPDATE_INFO_HPP

#include <string>
#include <optional>
#include <cstdint>

#include <libfilezilla/logger.hpp>

#include "../receiver/handle.hpp"
#include "../expected.hpp"
#include "../build_info.hpp"

namespace fz::update
{

struct allow
{
	enum type {
		// Ordered from the highest to the least priority
		release = 0,
		beta,

		everything = beta
	};

	allow() = default;
	allow(const allow &) = default;
	allow(allow &&) = default;
	allow &operator=(const allow &) = default;
	allow &operator=(allow &&) = default;

	allow(type t) noexcept
		: type_(t)
	{}

	operator type () const noexcept
	{
		return type_;
	}

	explicit operator bool () const noexcept
	{
		return release <= type_ && type_ <= beta;
	}

	constexpr explicit allow(std::string_view text) noexcept
	{
		if (text == "release")
			type_ = release;
		else
		if (text == "beta")
			type_ = beta;
	}

	template <typename String>
	friend String toString(allow a)
	{
		using C = typename String::value_type;

		if (a == release)
			return fzS(C, "release");

		if (a == beta)
			return fzS(C, "beta");

		return fzS(C, "unknown");
	}

	using serialization_alias = type;

private:
	type type_{};
};

class info final
{
public:
	class retriever;
	class raw_data_retriever;

	struct data {
		struct file_info
		{
			std::string url;
			std::int64_t size;
			std::string hash;
			allow type;

			bool operator==(const file_info &rhs) const
			{
				return std::tie(url, size, hash, type) == std::tie(rhs.url, rhs.size, rhs.hash, rhs.type);
			}
		};

		fz::build_info::version_info version;
		std::optional<file_info> file;
		std::string changelog;
		std::string signature;

		operator std::string() const;
		operator std::wstring() const;

		bool operator==(const data &rhs) const
		{
			return std::tie(version, file, changelog, signature) == std::tie(rhs.version, rhs.file, rhs.changelog, rhs.signature);
		}
	};

	info();

	info(const info&) = default;
	info(info&&) = default;
	info& operator=(const info&) = default;
	info& operator=(info&&) = default;

	bool operator==(const info &rhs) const
	{
		return data_ == rhs.data_;
	}

	bool operator!=(const info &rhs) const
	{
		return !(data_ == rhs.data_);
	}

	explicit operator bool() const
	{
		return bool(data_);
	}

	bool operator ==(const info &rhs);

	bool is_newer_than(const fz::build_info::version_info &vi) const
	{
		return data_ && data_->version && data_->version > vi;
	}

	bool is_eol() const
	{
		return data_ && !data_->version;
	}

	bool is_version() const
	{
		return data_ && data_->version && !data_->file;
	}

	bool is_release() const
	{
		return data_ && data_->version && data_->file && data_->file->type == allow::release;
	}

	bool is_beta() const
	{
		return data_ && data_->version && data_->file && data_->file->type == allow::beta;
	}

	bool is_allowed(allow allowed)
	{
		if (!data_) {
			return false;
		}

		if (!data_->file) {
			return true;
		}

		if (data_->file->type > allowed) {
			return false;
		}

		return true;
	}

	const data &operator*() const
	{
		return *data_;
	}

	const data *operator->() const
	{
		return &*data_;
	}

	data *operator->()
	{
		return &*data_;
	}

	fz::build_info::flavour_type get_flavour() const
	{
		return flavour_;
	}

	operator std::string() const;
	operator std::wstring() const;

	template <typename Archive>
	friend void save(Archive &ar, const info &i);

	template <typename Archive>
	friend void load(Archive &ar, info &i);

private:
	friend retriever;

	info(std::string_view raw_data, fz::build_info::flavour_type flavour, allow allowed, fz::logger_interface &logger);

	std::optional<data> data_;
	fz::build_info::flavour_type flavour_{fz::build_info::flavour};
};


class info::retriever
{
public:
	struct result_tag{};
	using result = receiver_event<result_tag, fz::expected<info, std::string /*error*/>>;

	virtual ~retriever();
	virtual void retrieve_info(bool manual, allow allowed, receiver_handle<result>) = 0;

protected:
	static info make_info(std::string_view raw_data, allow allowed, fz::logger_interface &logger);
};

class info::raw_data_retriever
{
public:
	struct result_tag{};
	using result = receiver_event<result_tag, fz::expected<std::string /*raw_data*/, std::string /*error*/>>;

	virtual ~raw_data_retriever();
	virtual void retrieve_raw_data(bool manual, receiver_handle<result>) = 0;
};

}
#endif // FZ_UPDATE_INFO_HPP
