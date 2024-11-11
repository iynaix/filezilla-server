#ifndef INTEGRALEDITOR_HPP
#define INTEGRALEDITOR_HPP

#include <libfilezilla/time.hpp>

#include <variant>
#include <any>

#include <wx/panel.h>

#include "eventex.hpp"

#include "../filezilla/util/traits.hpp"
#include "../filezilla/util/integral_ops.hpp"
#include "../filezilla/util/overload.hpp"
#include "../filezilla/enum_bitops.hpp"

class wxTextCtrl;
class wxCheckBox;
class wxStaticText;
class wxSpinButton;

enum IntegralEditorStyles: int
{
	IE_WITH_SPIN  = 1, ///< Enable spin button and arrow keys increase/decrease of the value
	IE_WRAPAROUND = 2  ///< If IE_WITH_SPIN is also set, makes it so that the value wraps around if one of the limits is reached
};

FZ_ENUM_BITOPS_DEFINE_FOR(IntegralEditorStyles)

class IntegralEditor: public wxPanel
{
	template <typename T, typename = void>
	class limited_ref_t;

	template <typename T>
	class limited_ref_t<T, std::enable_if_t<std::is_integral_v<T>>>: private std::reference_wrapper<T>
	{
		using map_t = std::vector<std::pair<T, wxString>>;

	public:
		using value_type = T;

		limited_ref_t(T &v, std::decay_t<T> min = std::numeric_limits<T>::min(), std::decay_t<T> max = std::numeric_limits<T>::max())
			: std::reference_wrapper<T>(v)
			, min_(min)
			, max_(max)
		{}

		bool increment(uintmax_t amount, uintmax_t scale);
		bool decrement(uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result from_string(const wxString &s, uintmax_t scale);
		wxString to_string(uintmax_t scale) const;

		void set_to_min();
		void set_to_max();

		bool has_map(const wxString &s) const;

		bool is_signed() const
		{
			return std::is_signed_v<T>;
		}

		limited_ref_t &set(T v)
		{
			this->get() = v;
			return *this;
		}

		using std::reference_wrapper<T>::get;

		limited_ref_t &set_mapping(const std::initializer_list<typename map_t::value_type> &list)
		{
			map_ = list;
			return *this;
		}

	private:
		T min_;
		T max_;
		std::vector<std::pair<T, wxString>> map_;
	};

	template <typename Dummy>
	class limited_ref_t<std::nullptr_t, Dummy>
	{
	public:
		using value_type = std::nullptr_t;

		limited_ref_t(){}
		limited_ref_t(std::nullptr_t){}

		bool increment(uintmax_t amount, uintmax_t scale);
		bool decrement(uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result from_string(const wxString &s, uintmax_t scale);
		wxString to_string(uintmax_t scale) const;

		void set_to_min();
		void set_to_max();

		bool has_map(const wxString &) const
		{
			return false;
		}

		bool is_signed() const
		{
			return false;
		}

		void set(std::nullptr_t){}
	};

	template <typename Dummy>
	class limited_ref_t<fz::duration, Dummy>: private std::reference_wrapper<fz::duration>
	{
	public:
		using value_type = fz::duration;

		limited_ref_t(fz::duration &v,
					  fz::duration min = fz::duration::from_milliseconds(std::numeric_limits<int64_t>::min()),
					  fz::duration max = fz::duration::from_milliseconds(std::numeric_limits<int64_t>::max()))
			: std::reference_wrapper<fz::duration>(v)
			, min_(min)
			, max_(max)
		{}

		bool increment(uintmax_t amount, uintmax_t scale);
		bool decrement(uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result from_string(const wxString &s, uintmax_t scale);
		wxString to_string(uintmax_t scale) const;

		void set_to_min();
		void set_to_max();

		bool is_signed() const
		{
			return true;
		}

		bool has_map(const wxString &) const
		{
			return false;
		}

		void set(fz::duration value)
		{
			this->get() = value;
		}

		using std::reference_wrapper<fz::duration>::get;

	private:
		fz::duration min_;
		fz::duration max_;
	};

	struct enum_wrapper
	{
		using map_t = std::vector<std::pair<std::any, wxString>>;

		template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>>* = nullptr, typename U = std::underlying_type_t<Enum>>
		enum_wrapper(Enum &e, Enum min = Enum(std::numeric_limits<U>::min()), Enum max = Enum(std::numeric_limits<U>::max()))
			: ptr_(&e)
			, min_(U(min))
			, max_(U(max))
			, increment_(&increment<Enum>)
			, decrement_(&decrement<Enum>)
			, from_string_(&from_string<Enum>)
			, to_string_(&to_string<Enum>)
			, set_to_min_(&set_to_min<Enum>)
			, set_to_max_(&set_to_max<Enum>)
			, is_signed_(&is_signed<Enum>)
		{}

		bool increment(uintmax_t amount, uintmax_t scale);
		bool decrement(uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result from_string(const wxString &s, uintmax_t scale);
		wxString to_string(uintmax_t scale) const;

		void set_to_min();
		void set_to_max();

		bool is_signed() const;

		bool has_map(const wxString &s) const;

		template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>>* = nullptr>
		bool set(Enum value)
		{
			if (auto *e = std::any_cast<Enum*>(&ptr_)) {
				**e = value;
				return true;
			}

			return false;
		}

		template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>>* = nullptr>
		bool get(Enum &value) const
		{
			if (auto e = std::any_cast<Enum*>(&ptr_)) {
				value = **e;
				return true;
			}

			return false;
		}

		enum_wrapper &set_mapping(const std::initializer_list<typename map_t::value_type> &list)
		{
			map_ = list;
			return *this;
		}

	private:
		std::any ptr_;
		std::any min_;
		std::any max_;
		std::vector<std::pair<std::any, wxString>> map_;

		bool (*increment_)(enum_wrapper &e, uintmax_t amount, uintmax_t scale);
		bool (*decrement_)(enum_wrapper &e, uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result (*from_string_)(enum_wrapper &e, const wxString &s, uintmax_t scale);
		wxString (*to_string_)(const enum_wrapper &e, uintmax_t scale);
		void (*set_to_min_)(enum_wrapper &e);
		void (*set_to_max_)(enum_wrapper &e);
		bool (*is_signed_)(const enum_wrapper &e);

		template <typename Enum>
		static bool increment(enum_wrapper &e, uintmax_t amount, uintmax_t scale)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);

			U tmp = U(ref);

			auto res = fz::util::increment(tmp, amount, scale, std::any_cast<U>(e.min_), std::any_cast<U>(e.max_));
			if (res)
				ref = Enum(tmp);

			return res;
		}

		template <typename Enum>
		static bool decrement(enum_wrapper &e, uintmax_t amount, uintmax_t scale)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);

			U tmp = U(ref);

			auto res = fz::util::decrement(tmp, amount, scale, std::any_cast<U>(e.min_), std::any_cast<U>(e.max_));
			if (res)
				ref = Enum(tmp);

			return res;
		}

		template <typename Enum>
		static fz::util::integral_op_result from_string(enum_wrapper &e, const wxString &s, uintmax_t scale)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);

			auto it = std::find_if(e.map_.begin(), e.map_.end(), [&s] (auto &p) {
				return s == p.second;
			});

			if (it != e.map_.end()) {
				ref = *std::any_cast<Enum>(&it->first);
				return fz::util::integral_op_result::ok;
			}

			U tmp = U(ref);

			auto res = fz::util::convert(s, tmp, scale, std::any_cast<U>(e.min_), std::any_cast<U>(e.max_));

			if (res)
				ref = Enum(tmp);

			return res;
		}

		template <typename Enum>
		static wxString to_string(const enum_wrapper &e, uintmax_t scale)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);

			auto it = std::find_if(e.map_.begin(), e.map_.end(), [&] (auto &p) {
				return std::underlying_type_t<Enum>(ref) / scale == std::underlying_type_t<Enum>(*std::any_cast<Enum>(&p.first)) / scale;
			});

			if (it != e.map_.end())
				return it->second;

			wxString s;

			fz::util::convert(U(ref), s, scale);
			return s;
		}

		template <typename Enum>
		static void set_to_min(enum_wrapper &e)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);
			auto min = std::any_cast<U>(e.min_);

			ref = Enum(min);
		}

		template <typename Enum>
		static void set_to_max(enum_wrapper &e)
		{
			using U = std::underlying_type_t<Enum>;
			auto &ref = *std::any_cast<Enum*>(e.ptr_);
			auto max = std::any_cast<U>(e.max_);

			ref = Enum(max);
		}

		template <typename Enum>
		static bool is_signed(const enum_wrapper &)
		{
			return std::is_signed_v<std::underlying_type_t<Enum>>;
		}
	};

	template <typename Dummy>
	class limited_ref_t<enum_wrapper, Dummy>: public enum_wrapper
	{
	public:
		using enum_wrapper::enum_wrapper;
	};

	template <typename T>
	class limited_value_base_t
	{
	protected:
		limited_value_base_t(const T &v)
			: v_(v)
		{}

		T v_;
	};

	template <typename T>
	class limited_ref_t<T&&, std::enable_if_t<std::is_integral_v<T>>>: private limited_value_base_t<T>, public limited_ref_t<T>
	{
	public:
		limited_ref_t(T &&v, std::decay_t<T> min = std::numeric_limits<T>::min(), std::decay_t<T> max = std::numeric_limits<T>::max())
			: limited_value_base_t<T>(v)
			, limited_ref_t<T>(limited_value_base_t<T>::v_ = v, min, max)
		{}
	};

	struct ref_t: std::variant<
		limited_ref_t<std::nullptr_t>,

		limited_ref_t<unsigned char>,
		limited_ref_t<unsigned short>,
		limited_ref_t<unsigned int>,
		limited_ref_t<unsigned long>,
		limited_ref_t<unsigned long long>,
		limited_ref_t<signed char>,
		limited_ref_t<signed short>,
		limited_ref_t<signed int>,
		limited_ref_t<signed long>,
		limited_ref_t<signed long long>,

		limited_ref_t<unsigned char&&>,
		limited_ref_t<unsigned short&&>,
		limited_ref_t<unsigned int&&>,
		limited_ref_t<unsigned long&&>,
		limited_ref_t<unsigned long long&&>,
		limited_ref_t<signed char&&>,
		limited_ref_t<signed short&&>,
		limited_ref_t<signed int&&>,
		limited_ref_t<signed long&&>,
		limited_ref_t<signed long long&&>,

		limited_ref_t<fz::duration>,
		limited_ref_t<enum_wrapper>
	>
	{
		using variant::variant;

		ref_t(const ref_t &) = delete;
		ref_t(ref_t &&) = default;

		ref_t &operator=(const ref_t &) = delete;
		ref_t &operator=(ref_t &&) = default;

		explicit operator bool() const;

		bool increment(uintmax_t amount, uintmax_t scale);
		bool decrement(uintmax_t amount, uintmax_t scale);
		fz::util::integral_op_result from_string(const wxString &s, uintmax_t scale);
		wxString to_string(uintmax_t scale) const;

		ref_t &set_to_min();
		ref_t &set_to_max();

		bool is_signed() const;
		bool has_map(const wxString &s) const;

		template <typename T, std::enable_if_t<!std::is_enum_v<T>>* = nullptr>
		auto get(T &v) const -> decltype(std::get_if<limited_ref_t<T>>(static_cast<const variant *>(this))->get(), bool())
		{
			auto ptr = std::get_if<limited_ref_t<T>>(static_cast<const variant *>(this));
			if (!ptr)
				return false;

			v = ptr->get();
			return true;
		}

		template <typename T, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
		auto get(T &v) const -> decltype(std::get_if<limited_ref_t<enum_wrapper>>(static_cast<const variant *>(this))->get(v))
		{
			auto ptr = std::get_if<limited_ref_t<enum_wrapper>>(static_cast<const variant *>(this));
			if (!ptr)
				return false;

			return ptr->get(v);
		}
	};

	template <typename T>
	struct setter
	{
		setter(IntegralEditor &owner, limited_ref_t<T> &lref)
			: owner_(owner)
			, lref_(lref)
		{}

		setter(setter &&) = delete;
		setter(const setter &) = delete;
		setter &operator=(setter &&) = delete;
		setter &operator=(const setter &) = delete;

		~setter()
		{
			owner_.SetRef(std::move(lref_));
		}

		limited_ref_t<T>* operator->() &&
		{
			return &lref_;
		}

	private:
		friend IntegralEditor;

		IntegralEditor &owner_;
		limited_ref_t<T> &lref_;
	};

private:
	bool TransferDataFromWindow() override;
	bool TransferDataToWindow() override;

	void modify(bool up, std::uintmax_t amount);

	wxSpinButton *spin_ctrl_{};
	wxTextCtrl *text_ctrl_{};
	wxStaticText *unit_ctrl_{};
	std::uintmax_t scale_{1};
	ref_t ref_;
	long style_{};

public:
	IntegralEditor() = default;

	bool Create(wxWindow *parent,
				const wxString &unit = wxEmptyString,
				std::uintmax_t scale = 1,
				long style = IE_WITH_SPIN | IE_WRAPAROUND,
				wxWindowID winid = wxID_ANY,
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				const wxString& name = wxS("integraleditor"));

	//! Sets the reference to the integral number the editor will have to display and let the user edit.
	//! Use nullptr in place of a "null reference".
	void SetRef(ref_t ref);

	template <typename T, typename... Args, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
	setter<enum_wrapper> SetRef(T &v, Args &&... args)
	{
		return {*this, ref_.emplace<limited_ref_t<enum_wrapper>>(v, std::forward<Args>(args)...) };
	}

	template <typename T, typename... Args, std::enable_if_t<!std::is_enum_v<T>>* = nullptr>
	setter<T> SetRef(T &v, Args &&... args)
	{
		return {*this, ref_.emplace<limited_ref_t<T>>(v, std::forward<Args>(args)...) };
	}

	template <typename T, typename... Args, std::enable_if_t<std::is_integral_v<T>>* = nullptr>
	setter<T&&> SetRef(T &&v, Args &&... args)
	{
		return {*this, ref_.emplace<limited_ref_t<T&&>>(std::forward<T>(v), std::forward<Args>(args)...) };
	}

	bool SetUnit(const wxString &name, std::uintmax_t scale);

	template <typename U, typename T>
	static bool SetValue(limited_ref_t<U> &ref, [[maybe_unused]] T v)
	{
		if constexpr (std::is_constructible_v<U, fz::util::underlying_t<T>>) {
			ref.set(U(v));
			return true;
		}

		return false;
	}

	template <typename T>
	static auto SetValue(limited_ref_t<enum_wrapper> &ref, T v) -> decltype(ref.set(v))
	{
		return ref.set(v);
	}


	template <typename T, std::enable_if_t<
		!std::is_same_v<std::nullptr_t, T>
	>* = nullptr>
	bool SetValue(T v)
	{
		bool success = std::visit([&v](auto &ref) {
			return SetValue(ref, v);
		}, static_cast<ref_t::variant &>(ref_));

		if (success)
			SetRef(std::move(ref_));

		return success;
	}

	template <typename T>
	auto Get(T &v) const -> decltype(ref_.get(v))
	{
		return ref_.get(v);
	}

	wxString ToString() const
	{
		return ref_.to_string(scale_);
	}

	void SetToMin()
	{
		ref_.set_to_min();
		SetRef(std::move(ref_));
	}

	void SetToMax()
	{
		ref_.set_to_max();
		SetRef(std::move(ref_));
	}

	void Increment(uintmax_t amount = 1)
	{
		ref_.increment(amount, scale_);
		SetRef(std::move(ref_));
	}

	void Decrement(uintmax_t amount = 1)
	{
		ref_.decrement(amount, scale_);
		SetRef(std::move(ref_));
	}

	void AutoComplete(const wxArrayString &array);

	struct Event: wxEventEx<Event>
	{
		inline const static Tag Changed;

	private:
		friend Tag;

		using wxEventEx::wxEventEx;
	};
};

#endif // INTEGRALEDITOR_HPP
