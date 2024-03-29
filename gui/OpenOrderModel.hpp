#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QLocale>
#include <QProcessEnvironment>
#include <QVector>

namespace cornus::gui {
class OpenOrderModel: public QAbstractTableModel {
public:
	OpenOrderModel(cornus::App *app, OpenOrderPane *oop);
	virtual ~OpenOrderModel();
	
	void AppendItem(DesktopFile *item);
	void ClearData();
	void RemoveItem(const int index);
	
	void SetData(QVector<DesktopFile*> &new_vec);
	
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	
	QVariant
	headerData(int section, Qt::Orientation orientation, int role) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override {
		return vec_.size();
	}
	int columnCount(const QModelIndex &parent = QModelIndex()) const override
	{ return 1; }
	
	void SetSelectedRow(const int row_index, const int old_row);
	void table(OpenOrderTable *p) { table_ = p; }
	void UpdateRowRange(int row1, int row2);
	void UpdateVisibleArea();
	
	QVector<DesktopFile*>& vec() { return vec_; }
	const QVector<DesktopFile*>& vec_const() const { return vec_; }
	
private:
	
	OpenOrderTable *table_ = nullptr;
	QVector<DesktopFile*> vec_;
	OpenOrderPane *oop_ = nullptr;
	QLocale locale_;
};
}
