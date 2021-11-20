#include "History.hpp"
#include "App.hpp"
#include "gui/Tab.hpp"
#include "gui/Table.hpp"
#include "gui/ToolBar.hpp"

namespace cornus {

/// #define CORNUS_DEBUG_HISTORY

History::History(App *app): app_(app) {}

History::~History() {}

void History::Add(const Action action, const QString &s)
{
	if (action == Action::Back || action == Action::Forward || action == Action::Reload)
		return;
	
	int last = vec_.size() - 1;
	if (last >= 0 && last < vec_.size()) {
		HistoryItem item = vec_[last];
		if (item.dir_path == s) {
			index_ = vec_.size() - 1;
#ifdef CORNUS_DEBUG_HISTORY
			auto ba = s.toLocal8Bit();
			mtl_trace("Not adding %s to index: %d", ba.data(), index_);
#endif
			app_->toolbar()->UpdateIcons(this);
			return;
		}
	}
	
	HistoryItem item;
	item.dir_path = s;
	vec_.append(item);
	index_ = vec_.size() - 1;
#ifdef CORNUS_DEBUG_HISTORY
	auto ba = s.toLocal8Bit();
	mtl_info("added: %s to index: %d", ba.data(), index_);
#endif
	
	if (index_ > 10)
		vec_.remove(0, 10);
	
	app_->toolbar()->UpdateIcons(this);
}

QString History::Back()
{
	if (index_ > 0)
		index_--;
	if (index_ < 0 || index_ >= vec_.size()) {
#ifdef CORNUS_DEBUG_HISTORY
		mtl_trace("index_: %d, size: %d", index_, vec_.size());
#endif
		return QString();
	}
	app_->toolbar()->UpdateIcons(this);
	HistoryItem &item = vec_[index_];
	return item.dir_path;
}

QString History::Forward()
{
	if (index_ < (vec_.size() - 1))
		index_++;
	if (index_ >= vec_.size()) {
#ifdef CORNUS_DEBUG_HISTORY
		mtl_trace("index_: %d, size: %d", index_, vec_.size());
#endif
		return QString();
	}
	
	app_->toolbar()->UpdateIcons(this);
	HistoryItem &item = vec_[index_];
	return item.dir_path;
}

void History::GetSelectedFiles(QVector<QString> &list)
{
	if (index_ < 0 || index_ >= vec_.size()) {
		return;
	}
	HistoryItem &item = vec_[index_];
	list = item.selected_filenames;
}

void History::Record()
{
	int last = index_;
	if (last >= 0 && last < vec_.size()) {
		app_->tab()->table()->GetSelectedFileNames(vec_[last].selected_filenames, StringCase::Lower);
	}
}

}


