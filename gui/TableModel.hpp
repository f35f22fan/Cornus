#pragma once

#include "decl.hxx"
#include "../decl.hxx"
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
	
	void DeleteSelectedFiles(const ShiftPressed sp);
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
	
	void SelectFilesLater(const QVector<QString> &v);
	void set_scroll_to_and_select(const QString &s) { scroll_to_and_select_ = s; }
	void SwitchTo(io::FilesData *new_data);
	gui::Tab* tab() const { return tab_; }
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
	void UpdateHeaderNameColumn();
	
public Q_SLOTS:
	void InotifyEvent(cornus::gui::FileEvent evt);
	void InotifyBatchFinished();
	
private:
	
	QString GetName() const;
	
	cornus::App *app_ = nullptr;
	gui::Tab *tab_ = nullptr;
	QString scroll_to_and_select_;
	
	/* When needing to select a file sometimes the file isn't yet in
	  the table_model's list because the inotify event didn't tell
	  it yet that a new file is available.
	 i16 holds the count how many times a given filename wasn't found
	 in the current list of files. When it happens a certain amount of
	 times the filename should be deleted from the hash - which is a
	 way to not allow the hash to grow uncontrollably by keeping
	 garbage.
	*/
	QHash<QString, i16> filenames_to_select_;// <filename, counter>
	
	mutable QString cached_free_space_;
	int tried_to_scroll_to_count_ = 0;
	int cached_row_count_ = 0;
};


}
