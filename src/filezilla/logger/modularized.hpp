#ifndef FZ_LOGGER_MODULARIZED_HPP
#define FZ_LOGGER_MODULARIZED_HPP

#include <string_view>
#include <unordered_map>
#include <optional>
#include <functional>

#include "hierarchical.hpp"

namespace fz::logger {

class modularized: public hierarchical_interface
{
public:
	using hierarchical_interface::hierarchical_interface;
	using meta_map = std::vector<std::pair<std::string, std::string>>;

	struct info
	{
		std::string name;
		meta_map meta;

		void set_meta(meta_map meta);
		void insert_meta(std::string key, std::string value);
		void erase_meta(const std::string &key);
		const std::string *find_meta(const std::string &key) const;
	};

	struct info_list: std::vector<info>
	{
		using to_string_f = std::function<std::string (const info &i, const info_list &parent_info_list)>;

		info_list();

		std::string as_string;

		class serializer;

	private:
		friend modularized;

		info_list(modularized *parent, std::string &&name, meta_map &&meta, to_string_f &&f);

		void update_string(const info_list &parent_info_list = none);

		to_string_f to_string_f_{};

		static std::string default_info_to_string(const info &i, const info_list &parent_info_list);
		static const info_list none;
	};

	modularized(const modularized &) = delete;
	modularized(modularized &&) = delete;

	modularized(logger_interface &parent, std::string name = {}, meta_map meta = {}, info_list::to_string_f f = {});
	modularized(modularized &parent, std::string name = {}, meta_map meta = {}, info_list::to_string_f f = {});

	void set_meta(meta_map meta);
	void insert_meta(std::string key, std::string value);
	void erase_meta(const std::string &key);
	std::optional<std::string> find_meta(const std::string &key) const;

	virtual void do_log(logmsg::type t, const info_list &info_list, std::wstring &&msg);

private:
	mutable fz::mutex mutex_;

	void do_log(logmsg::type t, std::wstring &&msg) override final;

	logger_interface *parent_{};
	modularized *modularized_parent_{};

	info_list info_list_;
};

}

#endif // FZ_LOGGER_MODULARIZED_HPP
