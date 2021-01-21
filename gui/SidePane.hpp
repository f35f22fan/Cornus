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

class SidePane : public QTableView {
	Q_OBJECT
public:
	SidePane(SidePaneModel *tm, App *app);
	virtual ~SidePane();
	
	gui::SidePaneItems& items() const;
	SidePaneModel* model() const { return model_; }
	QStyleOptionViewItem option() const { return viewOptions(); }
	
	virtual void dropEvent(QDropEvent *event) override;
	gui::SidePaneItem* GetItemAtNTS(const QPoint &pos, bool clone, int *ret_index = nullptr);
	void ProcessAction(const QString &action);
	void SelectProperPartition(const QString &full_path);
	void SelectRowSimple(const int row);
public slots:
	void DeselectAllItems(const int except_row, const bool row_flag, QVector<int> &indices);
	bool ScrollToAndSelect(QString name);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
private:
	NO_ASSIGN_COPY_MOVE(SidePane);
	
	void ShowRightClickMenu(const QPoint &pos);
	
	SidePaneModel *model_ = nullptr;
	App *app_ = nullptr;
	int drop_y_coord_ = -1;
	QPoint drag_start_pos_ = {-1, -1};
	
};

} // cornus::gui::
