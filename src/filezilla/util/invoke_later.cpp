#include "invoke_later.hpp"

fz::util::invoker_handler::invoker_handler(fz::event_loop &loop)
	: event_handler(loop)
{}

fz::util::invoker_handler::~invoker_handler()
{
	remove_handler();
}

