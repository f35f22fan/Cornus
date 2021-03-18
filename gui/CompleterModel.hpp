#pragma once

#include "decl.hxx"

#include <QAbstractTableModel>
#include <QVector>

namespace cornus::gui {

struct CompleterItems {
	QVector<QString> vec;
	QString dir_path;
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
	NO_ASSIGN_COPY_MOVE(CompleterModel);
	
	Location *location_ = nullptr;
	CompleterItems items_ = {};
};
}
