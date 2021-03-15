#pragma once

#include "decl.hxx"
#include "../MutexGuard.hpp"

#include <QAbstractTableModel>
#include <QVector>

#include <pthread.h>

namespace cornus::gui {

struct CompleterItems {
	///mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	QVector<QString> vec;
	QString dir_path;
	///MutexGuard guard() const { return MutexGuard(&mutex); }
};

class CompleterModel: public QAbstractTableModel {
public:
	CompleterModel(Location *location);
	virtual ~CompleterModel();
	
	void SetRootPath(QString root_path);
	
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const
		{ return 1; }
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:
	
	Location *location_ = nullptr;
	CompleterItems items_ = {};
};
}
