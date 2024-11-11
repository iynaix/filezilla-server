#ifndef FZ_UTIL_XML_ARCHIVER_HPP
#define FZ_UTIL_XML_ARCHIVER_HPP

#include <string>
#include <libfilezilla/time.hpp>
#include <libfilezilla/event_handler.hpp>

#include "../serialization/archives/xml.hpp"
#include "../util/locking_wrapper.hpp"

namespace fz::util {

class xml_archiver_base: protected fz::event_handler
{
protected:
	struct scoped_maybe_locker
	{
		scoped_maybe_locker(fz::mutex *mutex)
			: mutex_(mutex)
		{
			if (mutex_)
				mutex_->lock();
		}

		~scoped_maybe_locker()
		{
			if (mutex_)
				mutex_->unlock();
		}

		scoped_maybe_locker(const scoped_maybe_locker &) = delete;
		scoped_maybe_locker(scoped_maybe_locker &&) = delete;

		scoped_maybe_locker &operator=(const scoped_maybe_locker &) = delete;
		scoped_maybe_locker &operator=(scoped_maybe_locker &&) = delete;

	private:
		fz::mutex *mutex_;
	};

public:
	struct archive_info
	{
		std::string element_name;
		fz::native_string file_name;
	};

	enum event_dispatch_mode {
		do_not_dispatch,
		direct_dispatch,
		delayed_dispatch
	};

	using archive_result = fz::simple_event<xml_archiver_base, xml_archiver_base &, const archive_info &, int /*error*/>;

	xml_archiver_base(fz::event_loop &loop, fz::duration delay = fz::duration::from_milliseconds(100), fz::mutex *mutex = nullptr, fz::event_handler *target_handler = nullptr);
	~xml_archiver_base() override = 0; // To force implementation in derived class.

	virtual int save_now(event_dispatch_mode mode = delayed_dispatch) = 0;
	void save_later();
	void set_saving_delay(fz::duration delay);
	void set_event_handler(fz::event_handler *target_handler);

	const fz::native_string &get_file_name() const;
	const std::string &get_element_name() const;

private:
	void operator()(const fz::event_base &ev) override;

	fz::event_handler *target_handler_{};
	fz::duration delay_{};
	fz::timer_id timer_id_{};

protected:
	void dispatch_event(event_dispatch_mode mode, const archive_info &, int /*error*/);

	template <typename T>
	static int save(const T *v, const archive_info &info, event_dispatch_mode mode, xml_archiver_base *archiver)
	{
		if (!v)
			return 0;

		bool save_result = false;

		using namespace fz::serialization;
		xml_output_archive::file_saver saver(info.file_name);

		int error = xml_output_archive{saver, xml_output_archive::options().save_result(&save_result)}(nvp{*v, info.element_name.c_str()}).error();

		if (!error && !save_result)
			error = EIO;

		if (archiver)
			archiver->dispatch_event(mode, info, error);

		return error;
	}

	template <typename Tuple, std::size_t... Is>
	static int save_now(const std::index_sequence<Is...>&, const Tuple &tuple, event_dispatch_mode mode, xml_archiver_base *archiver)
	{
		int error = 0;

		static_cast<void>((!(error = save(std::get<Is>(tuple).first, std::get<Is>(tuple).second, mode, archiver)) && ...));

		return error;
	}

	template <typename T>
	static serialization::xml_input_archive::error_t load(T &v, const native_string &file_name, const std::string &element_name)
	{
		using namespace fz::serialization;

		xml_input_archive::file_loader loader(file_name);
		xml_input_archive ar{loader};

		if (!ar(nvp{v, element_name.c_str()}))
			return {ar.error().value(), ar.error().description()};

		return {};
	}

	fz::mutex *mutex_{};
};

template <typename T, typename... Ts>
class xml_archiver final: public xml_archiver_base
{
	using ptr_tuple_t = std::tuple<std::pair<T*, archive_info>, std::pair<Ts*, archive_info>...>;
	using const_ptr_tuple_t = std::tuple<std::pair<const T*, archive_info>, std::pair<const Ts*, archive_info>...>;

public:
	using xml_archiver_base::xml_archiver_base;

	template <std::size_t N = sizeof...(Ts), std::enable_if_t<N == 0>* = nullptr>
	xml_archiver(fz::event_loop &loop, T &v, std::string element_name, fz::native_string file_name, fz::duration delay, fz::mutex *mutex = nullptr, fz::event_handler *target_handler = nullptr)
		: xml_archiver_base(loop, delay, mutex, target_handler)
		, values_{{&v, {std::move(element_name), std::move(file_name)}}}
	{}

	void set_values(const std::pair<T&, archive_info> &v, const std::pair<Ts&, archive_info> &... vs)
	{
		scoped_maybe_locker lock(mutex_);

		values_ = { {&v.first, v.second}, {&vs.first, vs.second}... };
	}

	~xml_archiver() override
	{
		remove_handler();
	}

	template <std::size_t I = 0>
	locked_proxy<std::tuple_element_t<I, std::tuple<T, Ts...>>> lock()
	{
		return {std::get<I>(values_).first, mutex_};
	}

	int save_now(event_dispatch_mode mode) override
	{
		scoped_maybe_locker lock(mutex_);

		return xml_archiver_base::save_now(std::index_sequence_for<T, Ts...>(), values_, mode, this);
	}

	static int save_now(const std::pair<const T&, archive_info> &v, const std::pair<const Ts&, archive_info> &... vs)
	{
		return xml_archiver_base::save_now(std::index_sequence_for<T, Ts...>(), const_ptr_tuple_t{ {&v.first, v.second}, {&vs.first, vs.second}... }, do_not_dispatch, nullptr);
	}

	serialization::xml_input_archive::error_t load_into(T &v, Ts &...vs)
	{
		return load_into(std::index_sequence_for<T, Ts...>(), v, vs...);
	}

private:
	template <std::size_t I, std::size_t... Is>
	serialization::xml_input_archive::error_t load_into(const std::index_sequence<I, Is...> &, T &v,  Ts&...vs)
	{
		auto error = xml_archiver_base::load(v, std::get<I>(values_).second.file_name, std::get<I>(values_).second.element_name);

		if (!error)
			static_cast<void>((!(error = xml_archiver_base::load(vs..., std::get<Is>(values_).second.file_name, std::get<Is>(values_).second.element_name)) && ...));

		return error;
	}

	ptr_tuple_t values_{};
};

}

#endif // FZ_UTIL_XML_ARCHIVER_HPP
