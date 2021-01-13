#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {

class Table : public QTableView {
public:
	Table(TableModel *tm);
	virtual ~Table();
	
	TableModel* model() const { return table_model_; }
	virtual void dropEvent(QDropEvent *event) override;
	void ProcessAction(const QString &action);
	void ScrollToAndSelect(const QString &full_path);
	
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
	
	void RemoveSongsAndDeleteFiles(const QModelIndexList &indices);
	void ShowRightClickMenu(const QPoint &pos);
	void SortingChanged(int logical, Qt::SortOrder order);
	
	TableModel *table_model_ = nullptr;
	TableDelegate *delegate_ = nullptr;
	
	int drop_y_coord_ = -1;
	
};

} // cornus::gui::
