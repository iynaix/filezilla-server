#ifndef FZ_AUTHENTICATION_METHOD_HPP
#define FZ_AUTHENTICATION_METHOD_HPP

#include <climits>

#include <string>
#include <variant>
#include <bitset>

#include <libfilezilla/string.hpp>
#include <libfilezilla/format.hpp>

#include "../mpl/index_of.hpp"
#include "../mpl/contains.hpp"
#include "../mpl/for_each.hpp"
#include "../serialization/access.hpp"

#include "token_manager.hpp"

#define FZ_AUTHENTICATION_METHOD_NAME(_name)         \
	template <typename Char>                         \
	static inline const auto name = fzS(Char, _name) \
/***/

namespace fz::authentication {

namespace method {

struct none: std::monostate
{
	FZ_AUTHENTICATION_METHOD_NAME("none");
};

struct password
{
	std::string data;

	FZ_AUTHENTICATION_METHOD_NAME("password");
};

struct token
{
	refresh_token data;
	std::reference_wrapper<token_manager> manager;

	FZ_AUTHENTICATION_METHOD_NAME("token");
};

}

class any_method: public std::variant<
	authentication::method::none,
	authentication::method::password,
	authentication::method::token
>
{
public:
	using variant::variant;
	using variant::operator=;

	template <typename T>
	auto is() const noexcept -> decltype(std::get_if<T>(this))
	{
		return std::get_if<T>(this);
	}

	friend std::string to_string(const any_method &m);
	friend std::wstring to_wstring(const any_method &m);
};

class methods_list: public std::vector<any_method>
{
public:
	methods_list() = default;
	methods_list(std::initializer_list<any_method> l)
		: vector(l)
	{}

	methods_list(std::vector<any_method> v)
		: vector(std::move(v))
	{}

	methods_list(const methods_list &) = default;
	methods_list(methods_list &&) = default;

	methods_list& operator=(const methods_list &) = default;
	methods_list& operator=(methods_list &&) = default;

	bool just_verify() const
	{
		return just_verify_;
	}

protected:
	struct just_verify_tag{};

	methods_list(just_verify_tag, std::initializer_list<any_method> l)
		: vector(l)
		, just_verify_(true)
	{}

	methods_list(just_verify_tag, std::vector<any_method> v)
		: vector(std::move(v))
		, just_verify_(true)
	{}

private:
	bool just_verify_{};
};

class just_verify final: public methods_list
{
public:
	just_verify(std::initializer_list<any_method> l)
		: methods_list(just_verify_tag{}, l)
	{}
};

std::string to_string(const methods_list &ml);
std::wstring to_wstring(const methods_list &ml);

class methods_set
{
	static constexpr auto bits_size = std::variant_size_v<any_method::variant>-1;
	using bits = std::bitset<bits_size>;

	static_assert(bits_size <= sizeof(unsigned long long) * CHAR_BIT, "Number of bits too big to fit into a ULL");

public:
	template <typename T>
	static constexpr std::size_t idx = mpl::index_of_v<any_method::variant, T>-1;

	template <typename... Ts, std::enable_if_t<(mpl::contains_v<any_method::variant, Ts> &&...)>* = nullptr>
	methods_set(const Ts &...) noexcept
	{
		(add<Ts>(), ...);
	}

	methods_set(const any_method &m) noexcept
	{
		add(m);
	}

	methods_set(const methods_list &l) noexcept
	{
		for (auto &m: l)
			add(m);
	}

	explicit methods_set(unsigned long long v) noexcept
		: bits_(v)
	{
	}

	explicit methods_set(std::string_view v) noexcept;

	template <typename T>
	bool has() const noexcept
	{
		return bits_[idx<T>];
	}

	bool has(const any_method &vd) const noexcept
	{
		if (vd.index() > 0)
			return bits_[vd.index()-1];

		return false;
	}

	template <typename T>
	void erase() noexcept
	{
		bits_[idx<T>] = false;
	}

	void erase(const any_method &vd) noexcept
	{
		if (vd.index() > 0)
			bits_[vd.index()-1] = false;
	}

	void reset() noexcept
	{
		bits_.reset();
	}

	template <typename T>
	void add() noexcept
	{
		bits_[idx<T>] = true;
	}

	void add(const any_method &vd) noexcept
	{
		if (vd.index() > 0)
			bits_[vd.index()-1] = true;
	}

	methods_set operator &(const methods_set &rhs) const noexcept
	{
		return bits_ & rhs.bits_;
	}

	bool operator==(const methods_set &rhs) const noexcept
	{
		return bits_ == rhs.bits_;
	}

	std::size_t count() const noexcept
	{
		return bits_.count();
	}

	explicit operator bool() const noexcept
	{
		return bits_.count() != 0;
	}

	unsigned long long to_ullong() const
	{
		return bits_.to_ullong();
	}

	friend std::string to_string(const methods_set &set);
	friend std::wstring to_wstring(const methods_set &set);

private:
	methods_set(bits && bits)
		: bits_(std::move(bits))
	{}

	friend fz::serialization::access;

	template <typename Archive>
	void load_minimal(const Archive &, unsigned long long v)
	{
		using namespace fz::serialization;

		bits_ = bits(v);
	}

	template <typename Archive>
	unsigned long long save_minimal(const Archive &)
	{
		using namespace fz::serialization;

		return bits_.to_ullong();
	}

	bits bits_;
};

class available_methods: public std::vector<methods_set>
{
public:
	available_methods(std::initializer_list<methods_set> l)
		: vector(l)
	{}

	available_methods() = default;
	available_methods(const available_methods &) = default;
	available_methods(available_methods &&) = default;
	available_methods& operator=(const available_methods &) = default;
	available_methods& operator=(available_methods &&) = default;

	/// \brief returns a list of methods that match the provided set
	available_methods filter(const methods_set &set) const;

	/// \brief \returns true if the method list contains the specified set, false otherwise.
	/// \note The set must match exactly
	bool has(const methods_set &set) const;

	/// \brief \returns true if all the methods in the set are also in any of the list's sets, false otherwise.
	bool can_verify(const methods_set &method) const;

	/// \brief erases from all methods sets in the list, the method used to verify the data.
	/// It also removes from the list all the methods sets that DO NOT contain the method used to verify the data,
	/// effectively choosing a "validation route" among all the possible ones.
	///
	/// \returns true if auth is still necessary, false otherwise
	bool set_verified(const any_method &method);

	/// \brief \returns true if auth is necessary, false otherwise
	bool is_auth_necessary() const;

	/// \brief \returns true if auth is possible, false otherwise
	bool is_auth_possible() const;

	friend std::string to_string(const available_methods &);
	friend std::wstring to_wstring(const available_methods &);

	static const available_methods none;
};

}

#endif // FZ_AUTHENTICATION_METHOD_HPP
