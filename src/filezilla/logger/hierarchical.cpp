#include <cassert>

#include "hierarchical.hpp"
#include "type.hpp"

namespace fz::logger {

hierarchical_interface::hierarchical_interface(fz::logger_interface &logger)
	: parent_(dynamic_cast<hierarchical_interface *>(&logger))
{
	if (!parent_)
		return;

	fz::scoped_lock lock(parent_->mutex_);

	parent_->children_.push_back(*this);
}

hierarchical_interface::hierarchical_interface(hierarchical_interface &parent)
	: parent_(&parent)
{
	fz::scoped_lock lock(parent_->mutex_);

	parent_->children_.push_back(*this);
}

hierarchical_interface::~hierarchical_interface()
{
	if (!parent_)
		return;

	fz::scoped_lock lock1(parent_->mutex_);

#ifndef NDEBUG
	{
		fz::scoped_lock lock2(mutex_);
		assert(children_.empty() && "All children must be destroyed before their parent");
	}
#endif

	parent_->children_.remove(*this);
}

void hierarchical_interface::set_all(logmsg::type t)
{
	logger_interface::set_all(t);

	fz::scoped_lock lock(mutex_);

	for (auto &child: children_)
		child.set_all(t);
}

void hierarchical_interface::enable(logmsg::type t)
{
	logger_interface::enable(t);

	fz::scoped_lock lock(mutex_);

	for (auto &child: children_)
		child.enable(t);
}

void hierarchical_interface::disable(logmsg::type t)
{
	logger_interface::disable(t);

	fz::scoped_lock lock(mutex_);

	for (auto &child: children_)
		child.disable(t);
}

hierarchical_interface::hierarchical_interface()
	: parent_(nullptr)
{
	level_ |= logmsg::default_types;
}

}
