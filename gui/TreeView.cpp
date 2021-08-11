#include "TreeView.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "TreeItem.hpp"
#include "TreeModel.hpp"

#include <QDir>
#include <QMouseEvent>
#include <QProcess>

namespace cornus::gui {

bool PendingCommands::ContainsPath(const QString &path, const bool mark_expired)
{
	int i = -1;
	for (PendingCommand &cmd: pending_commands_)
	{
		i++;
		if (cmd.path == path) {
			if (mark_expired) {
				cmd.expired_bit(true);
			}
			return true;
		}
	}
	
	return false;
}

bool PendingCommands::ContainsFsUUID(const QString &fs_uuid, const bool mark_expired)
{
	for (PendingCommand &cmd: pending_commands_)
	{
		if (cmd.fs_uuid == fs_uuid) {
			if (mark_expired) {
				cmd.expired_bit(true);
			}
			return true;
		}
	}
	
	return false;
}

void PendingCommands::RemoveExpired()
{
	const auto now = time(NULL);
	int i = pending_commands_.size() - 1;
	for (; i >= 0; i--)
	{
		const PendingCommand &cmd = pending_commands_[i];
		if (cmd.expired_bit() || cmd.expires < now) {
			pending_commands_.remove(i);
		}
	}
}

TreeView::TreeView(App *app, TreeModel *model): app_(app), model_(model)
{
	Q_UNUSED(app_);
	setModel(model_);
	setDragEnabled(true);
	setAcceptDrops(true);
}

TreeView::~TreeView() {
	delete model_;
	delete menu_;
}

void TreeView::DeleteSelectedBookmark() {
	mtl_tbd();
}

void TreeView::ExecCommand(TreeItem *item, const PartitionEventType evt)
{
	CHECK_TRUE_VOID(item->is_partition());
	
	pending_commands_.vec().append(PendingCommand::New(
		evt,
		item->partition_info()->fs_uuid,
		item->mounted() ? item->mount_path() : QString())
	);
	
	const QString action_str = (evt == PartitionEventType::Mount) ?
		QLatin1String("mount") : QLatin1String("unmount");
	
	QStringList args;
	args << action_str;
	args << QLatin1String("-b");
	args << item->dev_path();
	QProcess::startDetached(QLatin1String("udisksctl"), args, QDir::homePath());
}

void TreeView::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTreeView::mouseDoubleClickEvent(evt);
}

void TreeView::mousePressEvent(QMouseEvent *evt)
{
	QTreeView::mousePressEvent(evt);
	const auto btn = evt->button();
	
	if (btn == Qt::LeftButton)
	{
		QModelIndex index = indexAt(evt->pos());
		if (!index.isValid())
			return;
		
		TreeItem *node = static_cast<TreeItem*>(index.internalPointer());
		CHECK_PTR_VOID(node);
		
		if (node->mounted() || node->is_bookmark()) {
			DirPath dp = { node->mount_path(), Processed::No };
			app_->GoTo(Action::To, dp);
		}
	} else if (btn == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPos(), evt->pos());
	}
}

void TreeView::mouseReleaseEvent(QMouseEvent *evt)
{
	QTreeView::mouseReleaseEvent(evt);
}

void TreeView::MarkCurrentPartition(const QString &full_path)
{
	TreeData &data = app_->tree_data();
	data.MarkCurrentPartition(full_path);
	update();
}

void TreeView::RenameSelectedBookmark()
{
	mtl_tbd();
}

void TreeView::ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos)
{
	auto indexes = selectedIndexes();
	if (indexes.isEmpty() || indexes.size() > 1)
		return;
	
	if (menu_ == nullptr)
		menu_ = new QMenu(this);
	else
		menu_->clear();
	
	TreeItem *node = static_cast<TreeItem*>(indexes[0].internalPointer());
	CHECK_PTR_VOID(node);
	
	if (node->is_bookmark())
	{
		QAction *action = menu_->addAction(tr("&Delete"));
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
		connect(action, &QAction::triggered, [=] {DeleteSelectedBookmark();});
		
		action = menu_->addAction(tr("&Rename.."));
		connect(action, &QAction::triggered, [=] { RenameSelectedBookmark();});
		action->setIcon(QIcon::fromTheme(QLatin1String("insert-text")));
	} else if (node->is_partition()) {
		if (node->mounted()) {
			QAction *action = menu_->addAction(tr("&Unmount"));
			action->setIcon(QIcon::fromTheme(QLatin1String("media-eject")));
			connect(action, &QAction::triggered, [=] () {
				ExecCommand(node, PartitionEventType::Unmount);
			});
		} else {
			QAction *action = menu_->addAction(tr("&Mount"));
			action->setIcon(QIcon::fromTheme(QLatin1String("media-eject")));
			connect(action, &QAction::triggered, [=] () {
				ExecCommand(node, PartitionEventType::Mount);
			});
		}
	}
	
	menu_->popup(global_pos);
}

}


