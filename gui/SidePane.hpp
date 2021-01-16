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
	SidePane(SidePaneModel *tm);
	virtual ~SidePane();
	
	SidePaneModel* model() const { return side_pane_model_; }
	virtual void dropEvent(QDropEvent *event) override;
	gui::SidePaneItem* GetItemAtNTS(const QPoint &pos, int *ret_index = nullptr);
	void ProcessAction(const QString &action);
	void SelectRowSimple(const int row);
public slots:
	bool ScrollToAndSelect(QString name);
	
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
	NO_ASSIGN_COPY_MOVE(SidePane);
	
	void ShowRightClickMenu(const QPoint &pos);
	
	SidePaneModel *side_pane_model_ = nullptr;
	
	int drop_y_coord_ = -1;
	
};

} // cornus::gui::
