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
namespace sidepane {
	void* LoadItems(void *args);
	QString ReadMountedPartitionFS(const QString &dev_path);
}

struct UpdateSidePaneArgs {
	QVector<int> indices;
	i32 prev_count = -1;
	i32 new_count = -1;
	i32 dir_id = -1;
};

struct InsertArgs {
	QVector<gui::SidePaneItem*> vec;
};
}

Q_DECLARE_METATYPE(cornus::gui::UpdateSidePaneArgs);
Q_DECLARE_METATYPE(cornus::gui::InsertArgs);

namespace cornus::gui {

class SidePaneModel: public QAbstractTableModel {
	Q_OBJECT
	
public:
	SidePaneModel(cornus::App *app);
	virtual ~SidePaneModel();
	
	cornus::App* app() const { return app_; }
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override
	{ return 1; }
	
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	
	void DeleteSelectedBookmarks();
	
	void
	FinishDropOperation(QVector<io::File*> *files_vec, int row);
	
	QVariant
	headerData(int section, Qt::Orientation orientation, int role) const override;
	
	QModelIndex
	index(int row, int column, const QModelIndex &parent) const override;
	
	bool InsertRows(const i32 at, const QVector<cornus::gui::SidePaneItem*> &items_to_add);
	
	virtual bool insertRows(int row, int count, const QModelIndex &parent) override {
		mtl_trace();
		return false;
	}
	
	virtual bool insertColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	void MoveBookmarks(QStringList str_list, const QPoint &pos);
	virtual bool removeRows(int row, int count, const QModelIndex &parent) override;
	virtual bool removeColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	
	void SetSidePane(cornus::gui::SidePane *p) { table_ = p; }
	void UpdateIndices(const QVector<int> indices);
	void UpdateRange(int row1, int row2);
	void UpdateSingleRow(int row) {
		UpdateRange(row, row);
	}
	void UpdateRowRange(int row_start, int row_end) {
		UpdateRange(row_start, row_end);
	}
	
	void UpdateVisibleArea();
	
public slots:
	void InsertFromAnotherThread(cornus::gui::InsertArgs args);
	
private:
	
	cornus::App *app_ = nullptr;
	cornus::gui::SidePane *table_ = nullptr;
};


}
