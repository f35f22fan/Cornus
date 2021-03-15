#pragma once

#include "BasicTable.hpp"
#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {

class SidePane : public BasicTable {
	Q_OBJECT
public:
	SidePane(SidePaneModel *tm, App *app);
	virtual ~SidePane();
	
	SidePaneItems& items() const;
	SidePaneModel* model() const { return model_; }
	QStyleOptionViewItem option() const { return viewOptions(); }
	
	virtual void dropEvent(QDropEvent *event) override;
	gui::SidePaneItem* GetItemAtNTS(const QPoint &pos, bool clone, int *ret_index = nullptr);
	inline int GetIconSize() { return verticalHeader()->defaultSectionSize(); }
	int GetSelectedBookmarkCount();
	i32 mouse_over_item_at() const { return mouse_over_item_at_; }
	void mouse_over_item_at(const i32 n) { mouse_over_item_at_ = n; }
	void ProcessAction(const QString &action);
	void SelectProperPartition(const QString &full_path);
	void SelectRowSimple(const int row, const bool skip_update = false);
	virtual void UpdateColumnSizes() override {}
public slots:
	void ClearHasBeenClicked(QString dev_path, QString error_msg);
	void DeselectAllItems(const int except_row, const bool row_flag, QVector<int> &indices);
	bool ScrollToAndSelect(QString name);
	void ReceivedPartitionEvent(cornus::PartitionEvent *p);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void leaveEvent(QEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void wheelEvent(QWheelEvent *evt) override;
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
private:
	NO_ASSIGN_COPY_MOVE(SidePane);
	
	void ClearDndAnimation(const QPoint &drop_coord);
	SidePaneItem* GetSelectedBookmark(int *index = nullptr);
	void HiliteFileUnderMouse();
	void MountPartition(SidePaneItem *partition);
	void RenameSelectedBookmark();
	void ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos);
	void StartDrag(const QPoint &pos);
	void UnmountPartition(int row);
	void UpdateLineHeight();
	
	SidePaneModel *model_ = nullptr;
	i32 mouse_over_item_at_ = -1;
	App *app_ = nullptr;
	QPoint drop_coord_ = {-1, -1};
	QPoint drag_start_pos_ = {-1, -1};
	QPoint mouse_pos_ = {-1, -1};
	QMenu *menu_ = nullptr;
	bool mouse_down_ = false;
	
};

} // cornus::gui::
