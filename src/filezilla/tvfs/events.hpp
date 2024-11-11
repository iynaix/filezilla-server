#ifndef FZ_TVFS_EVENTS_HPP
#define FZ_TVFS_EVENTS_HPP

#include <string>

#include <libfilezilla/fsresult.hpp>
#include "../receiver.hpp"

namespace fz::tvfs {

class entry;

struct completion_event_tag {};
using completion_event = receiver_event<completion_event_tag, result, std::string /*canonical path*/>;

struct simple_completion_event_tag{};
using simple_completion_event = receiver_event<simple_completion_event_tag, result>;

struct entry_result_tag{};
using entry_result = receiver_event<entry_result_tag, result, entry>;

}
#endif // EVENTS_HPP
