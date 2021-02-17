#pragma once

#include <QVector>

#include "decl.hxx"
#include "err.hpp"

namespace cornus {
class History {
public:
	History();
	virtual ~History();
	
	void Add(const Action action, const QString &s);
	QString Back();

private:
	NO_ASSIGN_COPY_MOVE(History);
	
	QVector<QString> paths_;
	i32 index_ = -1;
};
}
