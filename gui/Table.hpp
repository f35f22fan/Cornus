#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {

class Table : public QTableView {
	Q_OBJECT
public:
	Table(TableModel *tm);
	virtual ~Table();
	
	TableModel* model() const { return table_model_; }
	virtual void dropEvent(QDropEvent *event) override;
	io::File* GetFileAtNTS(const QPoint &pos, int *ret_file_index = nullptr);
	int GetFirstSelectedFile(io::File **ret_cloned_file);
	int GetSelectedFilesCount();
	void ProcessAction(const QString &action);
	void SelectAllFilesNTS(const bool flag, QVector<int> &indices);
	void SelectRowSimple(const int row);
public slots:
	bool ScrollToAndSelect(QString full_path);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
private:
	NO_ASSIGN_COPY_MOVE(Table);
	
	int IsOnFileNameStringNTS(const QPoint &pos, io::File **ret_file = nullptr);
	int SelectNextRow(const int relative_offset, const bool deselect_all_others, QVector<int> &indices); // returns newly selected row
	void ShowRightClickMenu(const QPoint &pos);
	void SortingChanged(int logical, Qt::SortOrder order);
	
	TableModel *table_model_ = nullptr;
	TableDelegate *delegate_ = nullptr;
	
	int drop_y_coord_ = -1;
	
};

} // cornus::gui::
