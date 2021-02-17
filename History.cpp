#include "History.hpp"

namespace cornus {

History::History() {}

History::~History() {}

void History::Add(const Action action, const QString &s)
{
	if (action == Action::Back || action == Action::Reload)
		return;
	paths_.append(s);
	index_ = paths_.size() - 1;
	
	if (index_ > 50)
		paths_.remove(0, 20);
}

QString History::Back()
{
	if (index_ > 0)
		index_--;
	if (index_ < 0 || index_ > paths_.size())
		return QString();
	
	return paths_[index_];
}

}


