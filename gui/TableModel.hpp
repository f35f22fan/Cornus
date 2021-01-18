#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"
#include "../types.hxx"

#include <QAbstractTableModel>
#include <QVector>

#include <sys/inotify.h>
#include <limits.h>
#include <type_traits>

namespace cornus::gui {

struct UpdateTableArgs {
	QVector<int> indices;
	i32 prev_count = -1;
	i32 new_count = -1;
	i32 dir_id = -1;
};
}
Q_DECLARE_METATYPE(cornus::gui::UpdateTableArgs);

namespace cornus::gui {

class TableModel: public QAbstractTableModel {
	Q_OBJECT
	
public:
	TableModel(cornus::App *app);
	virtual ~TableModel();
	
	cornus::App*
	app() const { return app_; }
	
	void DeleteSelectedFiles();
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	
	QVariant
	headerData(int section, Qt::Orientation orientation, int role) const override;
	
	QModelIndex
	index(int row, int column, const QModelIndex &parent) const override;
	
	bool InsertRows(const i32 at, const QVector<cornus::io::File *> &files_to_add);
	
	virtual bool insertRows(int row, int count, const QModelIndex &parent) override {
		mtl_trace();
		return false;
	}
	
	virtual bool insertColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	
	Notify& notify() { return notify_; }
	
	virtual bool removeRows(int row, int count, const QModelIndex &parent) override;
	virtual bool removeColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	
	void set_scroll_to_and_select(const QString &s) { scroll_to_and_select_ = s; }
	void SwitchTo(io::FilesData *new_data);
	void UpdateIndices(const QVector<int> &indices);
	void UpdateRange(int row1, Column c1, int row2, Column c2);
	void UpdateSingleRow(int row) {
		UpdateRange(row, Column::Icon, row, Column(i8(Column::Count) - 1));
	}
	void UpdateRowRange(int row_start, int row_end) {
		UpdateRange(row_start, Column::Icon, row_end,
			Column(i8(Column::Count) - 1));
	}
	
	void UpdateVisibleArea();
	
public slots:
	void UpdateTable(cornus::gui::UpdateTableArgs args);
	
private:
	
	cornus::App *app_ = nullptr;
	Notify notify_ = {};
	QString scroll_to_and_select_;
};


}
