#include "TreeView.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "RestorePainter.hpp"
#include "Tab.hpp"
#include "TreeItem.hpp"
#include "TreeModel.hpp"

#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QMouseEvent>
#include <QProcess>

namespace cornus::gui {

const QString BookmarkMime = QLatin1String("app/cornus_bookmark");

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
	setModel(model_);
	setDragEnabled(true);
	setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	// enables receiving ordinary mouse events (when mouse is not down)
	// setMouseTracking(true);
}

TreeView::~TreeView() {
	delete model_;
	delete menu_;
}

void TreeView::AnimateDND(const QPoint &drop_coord)
{
	// repaint() or update() don't work because
	// the window is not raised when dragging an item
	// on top of the table and the repaint
	// requests are ignored. Repaint using a hack:
	QModelIndex row = indexAt(drop_coord);
	
	if (!row.isValid())
		return;
	const int at = row.row();
	QModelIndex start = (at > 0) ? row.sibling(at - 1, 0) : row;
	QModelIndex end = row.sibling(at + 1, 0);
	
	if (!end.isValid())
		end = row;
	model_->UpdateIndices(start, end);
}

void TreeView::ExecCommand(TreeItem *item, const PartitionEventType evt)
{
	MTL_CHECK_VOID(item->is_partition());
	
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

void TreeView::dragEnterEvent(QDragEnterEvent *event)
{
	mouse_down_ = true;
	dragging_ = true;
	const QMimeData *mimedata = event->mimeData();
	
	if (mimedata->hasUrls() || !mimedata->data(BookmarkMime).isEmpty())
	{
		event->acceptProposedAction();
	}
}

void TreeView::dragLeaveEvent(QDragLeaveEvent *evt)
{
	AnimateDND(mouse_pos_);
	mouse_down_ = false;
	dragging_ = false;
	//setCursor(Qt::ArrowCursor);
}

void TreeView::dragMoveEvent(QDragMoveEvent *evt)
{
	mouse_pos_ = evt->position().toPoint();
	dragging_ = true;
	
	QModelIndex target = indexAt(mouse_pos_);
	bool ok = false;
	if (target.isValid())
	{
		TreeItem *item = static_cast<TreeItem*>(target.internalPointer());
		ok = item->is_bookmark() || item->is_bookmarks_root();
	}
	
	if (ok) {
		AnimateDND(mouse_pos_);
	} else {
		//model_->UpdateIndex(target);
	}
}

void TreeView::keyPressEvent(QKeyEvent *evt)
{
	const auto modifiers = evt->modifiers();
	const bool any_modifiers = (modifiers != Qt::NoModifier);
	const int key = evt->key();
	if (!any_modifiers)
	{
		if (key >= Qt::Key_1 && key <= Qt::Key_9) {
			const int index = key - Qt::Key_0 - 1;
			app_->SelectTabAt(index, FocusView::Yes);
			return;
		}
	}
}

void TreeView::dropEvent(QDropEvent *evt)
{
	//setCursor(Qt::ArrowCursor);
	mouse_down_ = false;
	dragging_ = false;
	
	if (evt->mimeData()->hasUrls())
	{
		QVector<io::File*> files_vec;
		
		for (const QUrl &url: evt->mimeData()->urls())
		{
			io::File *file = io::FileFromPath(url.path());
			if (file != nullptr)
				files_vec.append(file);
		}
		
		model_->AddBookmarks(files_vec, evt->position().toPoint());
	} else {
		QByteArray ba = evt->mimeData()->data(BookmarkMime);
		if (ba.isEmpty())
			return;
		
		QDataStream dataStreamRead(ba);
		QStringList str_list;
		dataStreamRead >> str_list;
		model_->MoveBookmarks(str_list, evt->position().toPoint());
	}
	
	AnimateDND(evt->position().toPoint());
}

TreeItem* TreeView::GetSelectedBookmark(QModelIndex *index)
{
	auto indexes = selectedIndexes();
	if (indexes.isEmpty() || indexes.size() > 1) {
		return nullptr;
	}
	TreeItem *node = static_cast<TreeItem*>(indexes[0].internalPointer());
	MTL_CHECK_ARG(node != nullptr, nullptr);
	if (node->is_bookmark()) {
		if (index)
			*index = indexes[0];
		return node;
	}
	delete node;
	return nullptr;
}

void TreeView::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTreeView::mouseDoubleClickEvent(evt);
}

void TreeView::mouseMoveEvent(QMouseEvent *evt)
{
	//HiliteFileUnderMouse();
	
	if (mouse_down_ && (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0))
	{
		StartDrag(evt->pos());
	}
}

void TreeView::mousePressEvent(QMouseEvent *evt)
{
	QTreeView::mousePressEvent(evt);
	mouse_down_ = true;
	drag_start_pos_ = evt->pos();
	const auto btn = evt->button();
	
	if (btn == Qt::LeftButton)
	{
		QModelIndex index = indexAt(evt->pos());
		if (!index.isValid())
			return;
		
		TreeItem *node = static_cast<TreeItem*>(index.internalPointer());
		MTL_CHECK_VOID(node != nullptr);
		
		if (node->mounted() || node->is_bookmark()) {
			DirPath dp = { node->mount_path(), Processed::No };
			app_->tab()->GoTo(Action::To, dp);
		}
	} else if (btn == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPosition().toPoint(), evt->position().toPoint());
	}
}

void TreeView::mouseReleaseEvent(QMouseEvent *evt)
{
	QTreeView::mouseReleaseEvent(evt);
	mouse_down_ = false;
}

void TreeView::MarkCurrentPartition(const QString &full_path)
{
	TreeData &data = app_->tree_data();
	data.MarkCurrentPartition(full_path);
	//update();
	model_->UpdateVisibleArea();
}

void TreeView::paintEvent(QPaintEvent *evt)
{
	QTreeView::paintEvent(evt);
	
	if (!mouse_down_ || !dragging_)
		return;
	
	QPainter painter(viewport());
	painter.setRenderHint(QPainter::Antialiasing);
	QPen pen(app_->green_color());
	painter.setPen(pen);
	
	const int y = mouse_pos_.y();
	painter.drawLine(0, y, width(), y);
}

void TreeView::RenameSelectedBookmark()
{
	TreeItem *item = GetSelectedBookmark();
	MTL_CHECK_VOID(item != nullptr);
	QString s = item->bookmark_name();
	gui::InputDialogParams params;
	params.initial_value = s;
	params.msg = tr("Edit Bookmark:");
	params.title = tr("Edit Bookmark");
	params.selection_start = 0;
	params.selection_end = s.size();
	
	QString ret_val;
	if (!app_->ShowInputDialog(params, ret_val)) {
		return;
	}
	
	ret_val = ret_val.trimmed();
	if (ret_val.isEmpty())
		return;
	
	QModelIndex index;
	TreeItem *sel_bkm = GetSelectedBookmark(&index);
	MTL_CHECK_VOID(sel_bkm != nullptr);
	sel_bkm->bookmark_name(ret_val);
	app_->SaveBookmarks();
	model_->UpdateIndex(index);
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
	MTL_CHECK_VOID(node != nullptr);
	
	if (node->is_bookmark())
	{
		QAction *action = menu_->addAction(tr("&Delete"));
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
		connect(action, &QAction::triggered, [=] {model_->DeleteSelectedBookmark();});
		
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

void TreeView::StartDrag(const QPoint &pos)
{
	auto diff = (pos - drag_start_pos_).manhattanLength();
	if (diff < QApplication::startDragDistance()) {
		return;
	}

	drag_start_pos_ = {-1, -1};
	QStringList str_list;
	auto indexes = selectedIndexes();
	if (indexes.isEmpty()) {
		return;
	}
	
	{
		for (QModelIndex &next: indexes)
		{
			TreeItem *node = static_cast<TreeItem*>(next.internalPointer());
			if (!node)
				continue;
			
			if (node->is_bookmark()) {
				str_list.append(node->bookmark_name());
				str_list.append(node->bookmark_path());
			}
		}
	}
	
	if (str_list.isEmpty()) {
		return;
	}
	QMimeData *mimedata = new QMimeData();
	QByteArray ba;
	QDataStream dataStreamWrite(&ba, QIODevice::WriteOnly);
	dataStreamWrite << str_list;
	mimedata->setData(BookmarkMime, ba);
	
	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimedata);
	drag->exec(Qt::MoveAction);
}

}


