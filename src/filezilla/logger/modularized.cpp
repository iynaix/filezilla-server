#include <cassert>

#include "modularized.hpp"
#include "../string.hpp"

namespace fz::logger {

modularized::modularized(fz::logger_interface &parent, std::string name, meta_map meta, info_list::to_string_f f)
	: hierarchical_interface(parent)
	, parent_(&parent)
	, modularized_parent_(dynamic_cast<modularized*>(&parent))
	, info_list_(modularized_parent_, std::move(name), std::move(meta), std::move(f))
{
	level_ = parent.levels();
}

modularized::modularized(modularized &parent, std::string name, modularized::meta_map meta, modularized::info_list::to_string_f f)
	: hierarchical_interface(parent)
	, parent_(&parent)
	, modularized_parent_(&parent)
	, info_list_(modularized_parent_, std::move(name), std::move(meta), std::move(f))
{
	level_ = parent.levels();
}

void modularized::set_meta(meta_map meta)
{
	fz::scoped_lock lock(mutex_);

	info_list_[0].set_meta(std::move(meta));

	if (modularized_parent_) {
		fz::scoped_lock lock(modularized_parent_->mutex_);
		info_list_.update_string(modularized_parent_->info_list_);
	}
	else
		info_list_.update_string();

	iterate_over_children([this](hierarchical_interface &r) {
		if (auto m = dynamic_cast<modularized*>(&r)) {
			fz::scoped_lock lock(m->mutex_);
			std::copy(info_list_.begin(), info_list_.end(), std::next(m->info_list_.begin()));
			m->info_list_.update_string(info_list_);
		}

		return true;
	});
}

void modularized::insert_meta(std::string key, std::string value)
{
	fz::scoped_lock lock(mutex_);

	info_list_[0].insert_meta(std::move(key), std::move(value));

	set_meta(std::move(info_list_[0].meta));
}

void modularized::erase_meta(const std::string &key)
{
	fz::scoped_lock lock(mutex_);

	info_list_[0].erase_meta(key);

	set_meta(std::move(info_list_[0].meta));
}

std::optional<std::string> modularized::find_meta(const std::string &key) const
{
	fz::scoped_lock lock(mutex_);

	if (auto ret = info_list_[0].find_meta(key))
		return *ret;

	return {};
}

void modularized::do_log(logmsg::type t, const info_list &l, std::wstring &&msg)
{
	if (!parent_ || !parent_->should_log(t))
		return;

	if (modularized_parent_)
		modularized_parent_->do_log(t, l, std::move(msg));
	else
	if (l.as_string.empty())
		parent_->do_log(t, std::move(msg));
	else
		parent_->do_log(t, fz::sprintf(L"[%s] %s", l.as_string, std::move(msg)));
}

void modularized::do_log(logmsg::type t, std::wstring &&msg)
{
	fz::scoped_lock lock(mutex_);

	for (auto l: strtokenizer(msg, L"\n", false)) {
		if (l.size() == msg.size()) {
			// If there was no newline, no need to create a new string.
			do_log(t, info_list_, std::move(msg));
			break;
		}

		// Use a thread-local so memory allocated by the wstring has a chance to get reused
		thread_local std::wstring line;
		line = l;

		do_log(t, info_list_, std::move(line));
	}
}

void modularized::info::set_meta(meta_map m)
{
	bool can_set_meta = meta.size() == 0 || !name.empty();

	assert(can_set_meta && "If the name is empty, also the meta map must be empty");

	if (!can_set_meta)
		return;

	meta = std::move(m);
}

void modularized::info::insert_meta(std::string key, std::string value)
{
	auto it = std::find_if(meta.begin(), meta.end(), [&](auto &p) {
		return p.first == key;
	});

	if (it != meta.end())
		it->second = std::move(value);
	else
		meta.emplace_back(std::move(key), std::move(value));
}

void modularized::info::erase_meta(const std::string &key)
{
	auto it = std::find_if(meta.begin(), meta.end(), [&](auto &p) {
		return p.first == key;
	});

	if (it != meta.end())
		meta.erase(it);
}

const std::string *modularized::info::find_meta(const std::string &key) const
{
	auto it = std::find_if(meta.begin(), meta.end(), [&](auto &p) {
		return p.first == key;
	});

	if (it != meta.end())
		return &it->second;

	return {};
}

modularized::info_list::info_list()
{
}

modularized::info_list::info_list(modularized *p, std::string &&name, modularized::meta_map &&meta, to_string_f &&f)
	: to_string_f_(std::move(f))
{
	if (!to_string_f_)
		to_string_f_ = default_info_to_string;

	if (p) {
		fz::scoped_lock lock(p->mutex_);

		reserve(p->info_list_.size()+1);

		push_back({std::move(name), std::move(meta)});
		std::copy(p->info_list_.begin(), p->info_list_.end(), std::back_inserter(*this));
		update_string(p->info_list_);

		return;
	}

	push_back({std::move(name), std::move(meta)});
	update_string();
}

void modularized::info_list::update_string(const info_list &parent_info_list)
{
	as_string = to_string_f_((*this)[0], parent_info_list);
}

std::string modularized::info_list::default_info_to_string(const info &i, const info_list &parent_info_list)
{
	static const std::vector<std::pair<std::string_view, std::string_view>> escape_map = {
		{ "C", "," },
		{ "S", "/" },
		{ "D", ":" },
		{ "L", "[" },
		{ "R", "]" }
	};

	std::string ret;

	if (i.name.empty())
		ret = parent_info_list.as_string;
	else
	if (parent_info_list.as_string.empty())
		ret = fz::escaped(i.name, "%", escape_map);
	else
		ret = std::string(parent_info_list.as_string).append(1, '/').append(fz::escaped(i.name, "%", escape_map));

	for (auto &p: i.meta) {
		ret.append(", ")
		   .append(fz::escaped(p.first, "%", escape_map))
		   .append(": ")
		   .append(fz::escaped(p.second, "%", escape_map));
	}

	return ret;
}

const modularized::info_list modularized::info_list::none;

}
