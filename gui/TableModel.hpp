#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"
#include "../types.hxx"

#include <QAbstractTableModel>
#include <QVector>

#include <type_traits>

namespace cornus::gui {

class TableModel: public QAbstractTableModel {
	Q_OBJECT
public:
	TableModel(cornus::App *app);
	virtual ~TableModel();
	
	cornus::App*
	app() const { return app_; }
	
	int
	rowCount(const QModelIndex &parent = QModelIndex()) const override;
	
	int
	columnCount(const QModelIndex &parent = QModelIndex()) const override;
	
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	
	cornus::io::Files*
	files() { return files_; }
	
	QVariant
	headerData(int section, Qt::Orientation orientation, int role) const override;
	
	QModelIndex
	index(int row, int column, const QModelIndex &parent) const override;
	
	bool
	InsertRows(const i32 at, const QVector<cornus::io::File *> &files_to_add);
	
	virtual bool insertRows(int row, int count, const QModelIndex &parent) override {
		mtl_trace();
		return false;
	}
	
	virtual bool insertColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	virtual bool removeRows(int row, int count, const QModelIndex &parent) override;
	virtual bool removeColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	
	void SwitchTo(io::Files *files);
	
	void
	UpdateRange(int row1, Column c1, int row2, Column c2);
	
	void
	UpdateRangeDefault(int row) {
		UpdateRange(row, Column::FileName, row, Column::Size);
	}
	
	void
	UpdateRowRange(int row_start, int row_end) {
		UpdateRange(row_start, Column::FileName, row_end,
			Column(i8(Column::Count) - 1));
	}
	
private:
	
	cornus::App *app_ = nullptr;
	mutable cornus::io::Files *files_ = nullptr;
};


}
