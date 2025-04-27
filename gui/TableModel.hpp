#pragma once

#include "decl.hxx"
#include "../CondMutex.hpp"
#include "../decl.hxx"
#include "../ElapsedTimer.hpp"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"

#include <QAbstractTableModel>
#include <QVector>

#include <sys/inotify.h>
#include <limits.h>
#include <type_traits>

namespace cornus::gui {

class TableModel: public QAbstractTableModel
{
	Q_OBJECT
public:
	TableModel(cornus::App *app, gui::Tab *tab);
	virtual ~TableModel();
	
	cornus::App*
	app() const { return app_; }
	
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
	
	virtual bool removeRows(int row, int count, const QModelIndex &parent) override;
	virtual bool removeColumns(int column, int count, const QModelIndex &parent) override {
		mtl_trace();
		return true;
	}
	
	CondMutex* reload_meta_cm() { return &reload_meta_cm_; }
	
	void SwitchTo(io::FilesData *new_data);
	gui::Tab* tab() const { return tab_; }
	void UpdateIndices(const QSet<int> &indices);
	void UpdateRange(int row1, Column c1, int row2, Column c2);
	void UpdateSingleRow(int row) {
		UpdateRange(row, Column::Icon, row, Column(i8(Column::Count) - 1));
	}
	void UpdateFileIndexRange(int row_start, int row_end) {
		UpdateRange(row_start, Column::Icon, row_end,
			Column(i8(Column::Count) - 1));
	}
	
	void UpdateVisibleArea();
	void UpdateHeaderNameColumn();
	
public Q_SLOTS:
	void InotifyEventInGuiThread(cornus::io::FileEvent evt);
	void SelectFilesAfterInotifyBatch();
	void UpdatedFilesArrived(QList<io::File*> needles);
	void UpdateProxy();
private:
	
	QString GetName() const;
	void InsertFile(io::File *new_file);
	void RemoveFile(cint index);
	
	cornus::App *app_ = nullptr;
	gui::Tab *tab_ = nullptr;
	
	int update_index_ = -1;
	
	mutable QString cached_free_space_;
	int tried_to_scroll_to_count_ = 0;
	CondMutex reload_meta_cm_ = {};
};


}
