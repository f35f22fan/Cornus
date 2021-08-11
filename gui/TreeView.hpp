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
	void MarkCurrentPartition(const QString &full_path);
	QStyleOptionViewItem option() const { return viewOptions(); }
	void ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos);
	
	PendingCommands& pending_commands() { return pending_commands_; }
	
protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;

private:
	void DeleteSelectedBookmark();
	void RenameSelectedBookmark();
	
	App *app_ = nullptr;
	TreeModel *model_ = nullptr;
	QMenu *menu_ = nullptr;
	PendingCommands pending_commands_;
};
}
