#pragma once

#include "../decl.hxx"
#include "decl.hxx"

#include <QTreeView>

namespace cornus::gui {

class PendingCommands {
public:
	bool ContainsPath(const QString &path, const bool mark_expired);
	bool ContainsFsUUID(const QString &fs_uuid, const bool mark_expired);
	void RemoveExpired();
	QVector<PendingCommand>& vec() { return pending_commands_; }
private:
	QVector<PendingCommand> pending_commands_;
};

class TreeView: public QTreeView {
	Q_OBJECT
public:
	explicit TreeView(App *app, TreeModel *model);
	virtual ~TreeView();
	
	void ExecCommand(TreeItem *item, const PartitionEventType evt);
	TreeItem* GetSelectedBookmark(QModelIndex *index = nullptr);
	void MarkCurrentPartition(const QString &full_path);
	QStyleOptionViewItem option() const { return QStyleOptionViewItem(); }
	int RowHeight(const QModelIndex &i) const { return rowHeight(i); }
	void ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos);
	
	PendingCommands& pending_commands() { return pending_commands_; }
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
	
	void keyPressEvent(QKeyEvent *evt) override;
	
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	
	virtual void paintEvent(QPaintEvent *evt) override;

private:
	void AnimateDND(const QPoint &drop_coord);
	void StartDrag(const QPoint &pos);
	void RenameSelectedBookmark();
	
	App *app_ = nullptr;
	TreeModel *model_ = nullptr;
	QMenu *menu_ = nullptr;
	PendingCommands pending_commands_;
	QPoint mouse_pos_ = {-1, -1}, drag_start_pos_ = {-1, -1};
	bool mouse_down_ = false, dragging_ = false;
	QCursor cursor_;
	Qt::CursorShape cursor_shape_ = Qt::ArrowCursor;
};
}
