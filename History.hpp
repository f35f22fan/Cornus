#pragma once

#include <QVector>

#include "decl.hxx"
#include "err.hpp"

namespace cornus {

struct HistoryItem {
	QString dir_path;
	QVector<QString> selected_filenames;
};

class History {
public:
	History(App *app);
	virtual ~History();
	
	void Add(const Action action, const QString &s);
	QString Back();
	QString Forward();
	void GetSelectedFiles(QVector<QString> &list);
	void Record();
	void index_size(int &index, int &size) {
		index = index_, size = vec_.size();
	}
	QVector<QString>& recorded() { return recorded_; }
	
private:
	NO_ASSIGN_COPY_MOVE(History);
	
	QVector<HistoryItem> vec_;
	QVector<QString> recorded_;
	i4 index_ = -1;
	App *app_ = nullptr;
};
}
