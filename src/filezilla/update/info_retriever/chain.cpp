#include "../../receiver/async.hpp"
#include "../../util/demangle.hpp"

#include "chain.hpp"

namespace fz::update::info_retriever {

chain::chain(logger_interface &logger, std::initializer_list<std::reference_wrapper<info::raw_data_retriever>> retrievers)
	: chain(logger, make_factory(retrievers))
{}

chain::chain(logger_interface &logger, raw_data_retrievers_factory factory)
	: logger_(logger, "Update Chain Retriever")
	, factory_(std::move(factory))
{}

void chain::set_expiration(duration expiration)
{
	expiration_ = expiration;
}

void chain::retrieve_info(bool manual, allow allowed, fz::receiver_handle<result> h)
{
	// We need to make a heap-copy of the retrievers so that if they are changed while we're performing a retrieval
	// things will keep working
	auto retrievers = std::make_unique<raw_data_retrievers>(factory_ ? factory_(manual) : raw_data_retrievers());

	auto it = std::find_if(retrievers->cbegin(), retrievers->cend(), [](const auto *r) { return r != nullptr; });
	if (it == retrievers->cend()) {
		logger_.log_raw(logmsg::debug_warning, L"No retrievers were set");
		return h(fz::unexpected("No retrievers were set"));
	}

	logger_.log_u(logmsg::debug_debug, L"Invoking retriever %s.", util::invoke_later(util::demangle, typeid(**it).name()));
	(*it)->retrieve_raw_data(manual, async_reentrant_receive(h) >> [retrievers = std::move(retrievers), it, manual,  allowed, h = std::move(h), cumulative_errors = std::string(), this](auto &&self, auto &expected_result) mutable {
		if (!expected_result) {
			if (!cumulative_errors.empty())
				cumulative_errors += '\n';

			logger_.log_u(logmsg::debug_warning, L"Got error from retriever %s: %s", util::invoke_later(util::demangle, typeid(**it).name()), expected_result.error());
			cumulative_errors += expected_result.error();
		}
		else
		if (const auto &info = make_info(*expected_result, allowed, logger_); !info)
			logger_.log_u(logmsg::debug_warning, L"Got no updater info from retriever %s.", util::invoke_later(util::demangle, typeid(**it).name()));
		else
			return h(std::move(info));

		it = std::find_if(++it, retrievers->cend(), [](const auto *r) { return r != nullptr; });
		if (it == retrievers->cend()) {
			if (cumulative_errors.empty())
				return h(info());

			return h(fz::unexpected(std::move(cumulative_errors)));
		}

		logger_.log_u(logmsg::debug_debug, L"Invoking retriever %s.", util::invoke_later(util::demangle, typeid(**it).name()));
		(*it)->retrieve_raw_data(manual, std::move(self));
	});
}

void chain::set_retrievers(std::initializer_list<std::reference_wrapper<info::raw_data_retriever>> retrievers)
{
	factory_ = make_factory(retrievers);
}

void chain::set_retrievers_factory(raw_data_retrievers_factory factory)
{
	factory_ = std::move(factory);
}

chain::raw_data_retrievers_factory chain::make_factory(std::initializer_list<std::reference_wrapper<info::raw_data_retriever>> retrievers)
{
	raw_data_retrievers result;
	result.reserve(retrievers.size());

	for (auto r: retrievers)
		result.push_back(&r.get());

	return [result](auto){
		return result;
	};
}


}
