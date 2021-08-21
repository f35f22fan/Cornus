#include "TreeModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/File.hpp"
#include "../Prefs.hpp"
#include "sidepane.hh"
#include "../TreeData.hpp"
#include "TreeItem.hpp"
#include "TreeView.hpp"

#include <QDir>
#include <QItemSelectionModel>
#include <QScrollBar>

namespace cornus::gui {

TreeModel::TreeModel(cornus::App *app, QObject *parent)
: QAbstractItemModel(parent), app_(app)
{
	SetupModelData();
}

TreeModel::~TreeModel()
{}

int TreeModel::columnCount(const QModelIndex &parent_index) const
{
	return 1;
	
//	if (!parent_index.isValid())
//		return root_->column_count();
	
//	auto *node = static_cast<TreeItem*>(parent_index.internalPointer());
//	return node->column_count();
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();
	
	TreeItem *node = static_cast<TreeItem*>(index.internalPointer());
	
	if (role == Qt::DisplayRole)
	{
		return node->data(index.column());
	} else if (role == Qt::FontRole) {
		QFont font;
		
		if (node->mounted())
			font.setBold(true);
		
		if (node->current_partition()) {
			font.setUnderline(true);
		}
		
		return font;
	} else if (role == Qt::ForegroundRole) {
		if (node->HasRootPartition()) {
			return app_->green_color();
		} else if (node->is_partition()) {
			if (node->mounted() && node->mount_path() == QChar('/'))
				return app_->green_color();
		}
	}
	
	return QVariant();
}

void TreeModel::DeleteSelectedBookmark()
{
	TreeData &data = app_->tree_data();
	QModelIndex root_index;
	{
		TreeItem *root = data.GetBookmarksRoot();
		CHECK_PTR_VOID(root);
		QModelIndex child_index;
		TreeItem *item = view_->GetSelectedBookmark(&child_index);
		CHECK_PTR_VOID(item);
		root_index = createIndex(root->Row(), 0, root);
		const int at = child_index.row();
		{
			beginRemoveRows(root_index, at, at);
			root->DeleteChild(item);
			endRemoveRows();
		}
	}
	
	app_->SaveBookmarks();
}

void TreeModel::DeviceEvent(const Device device, const DeviceAction action,
	const QString dev_path, const QString sys_path)
{
	TreeData &data = app_->tree_data();
	struct udev *udev = udev_new();
	CHECK_PTR_VOID(udev);
	UdevAutoUnref auto_unref(udev);
	CHECK_TRUE_VOID(((action == DeviceAction::Added) || (action == DeviceAction::Removed)));
	
	if (device == Device::Disk)
	{
		if (action == DeviceAction::Added)
		{
			auto sys_path_ba = sys_path.toLocal8Bit();
			struct udev_device *device = udev_device_new_from_syspath(udev, sys_path_ba.data());
			CHECK_PTR_VOID(device);
			UdevDeviceAutoUnref auto_unref_device(device);
			
			const int at = data.roots.size();
			TreeItem *root = TreeItem::NewDisk(device, at);
			auto index = QModelIndex();
			beginInsertRows(index, at, at);
			{
				data.roots.append(root);
			}
			endInsertRows();
		} else if (action == DeviceAction::Removed) {
			// Creating a udev_device from sys_path here will fail
			// because the device is removed.
			int i = -1;
			for (TreeItem *root: data.roots)
			{
				i++;
				if (root->is_disk() && root->disk_info().dev_path == dev_path) {
					beginRemoveRows(QModelIndex(), i, i);
					{
						data.roots.remove(i);
						delete root;
					}
					endRemoveRows();
					break;
				}
			}
		}
		return;
	}
	
	CHECK_TRUE_VOID((device == Device::Partition));
	
	if (action == DeviceAction::Added) {
		auto sys_path_ba = sys_path.toLocal8Bit();
		struct udev_device *device = udev_device_new_from_syspath(udev, sys_path_ba.data());
		CHECK_PTR_VOID(device);
		UdevDeviceAutoUnref auto_unref_device(device);
		TreeItem *child = TreeItem::FromDevice(device, nullptr);
		InsertPartition(child);
	} else if (action == DeviceAction::Removed) {
		bool done = false;
		{
			for (TreeItem *root: data.roots)
			{
				if (!root->is_disk())
					continue;
				auto &vec =root->children();
				const int count = vec.size();
				
				for (int i = 0; i < count; i++)
				{
					TreeItem *child = vec[i];
					PartitionInfo *info = child->partition_info();
					CHECK_PTR_VOID(info);
					if (info->dev_path == dev_path)
					{
						auto parent_index = createIndex(root->Row(), 0, root);
						{
							beginRemoveRows(parent_index, i, i);
							{
								vec.remove(i);
								delete child;
								done = true;
							}
							endRemoveRows();
						}
						break;
					}
				}
				
				if (done)
					break;
			}
		}
	}
}

bool TreeModel::FinishDropOperation(QVector<io::File*> *files_vec,
	const QPoint &widget_pos)
{
	AutoDeleteVecP advp(files_vec);
	QModelIndex target;
	const int start_at = GetDropLocation(widget_pos, target);
	if (start_at == -1)
		return false;
	
	TreeData &data = app_->tree_data();
	{
		TreeItem *item = static_cast<TreeItem*>(target.internalPointer());
		if (!item->is_bookmark() && !item->is_bookmarks_root()) {
			mtl_trace();
			return false;
		}
		
	}
	
	for (int i = files_vec->size() - 1; i >= 0; i--)
	{
		io::File *next = (*files_vec)[i];
		if (!next->is_dir_or_so()) {
			delete files_vec->takeAt(i);
		}
	}
	
	if (files_vec->isEmpty())
		return false;
	
	TreeItem *bookmarks_root = data.GetBookmarksRoot();
	CHECK_PTR(bookmarks_root);
	QModelIndex root_index = createIndex(bookmarks_root->Row(), 0, bookmarks_root);
	
	const int count = files_vec->size();
	const int start_row = start_at;
	
	for (int i = 0; i < count; i++)
	{
		io::File *next = (*files_vec)[i];
		const int at = start_row + i;
		beginInsertRows(root_index, at, at);
		{
			if (next->is_dir_or_so())
			{
				TreeItem *new_item = TreeItem::NewBookmark(*next);
				bookmarks_root->InsertChild(new_item, at);
			}
		}
		endInsertRows();
	}
	
	app_->SaveBookmarks();
	
	return true;
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	
	return QAbstractItemModel::flags(index);
}

int TreeModel::GetDropLocation(const QPoint &pos, QModelIndex &target)
{
	target = view_->indexAt(pos);
	if (!target.isValid())
		return -1;
	const int rh = view_->RowHeight(target);
	const int y = pos.y();
	const int rem = y % rh;
	const int at = target.row();
	return (rem > rh / 2) ? at + 1 : at;
}

gui::TreeItem* TreeModel::GetRootTS(const io::DiskInfo &disk_info, int *row)
{
	TreeData &data = app_->tree_data();
	int count = -1;
	{
		int i = -1;
		for (gui::TreeItem *root: data.roots) {
			i++;
			if (!root->is_disk())
				continue;
			const auto &n = root->disk_info().num;
			if (n == disk_info.num) {
				if (row)
					*row = i;
				return root;
			}
		}
		count = data.roots.size();
	}
	
	const int at = count;
	auto *disk_root = gui::TreeItem::NewDisk(disk_info, at);
	beginInsertRows(QModelIndex(), at, at);
	{
		data.roots.append(disk_root);
	}
	endInsertRows();
	
	if (row)
		*row = at;
	
	return disk_root;
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	//if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		//return root_->data(section);
	
	return QVariant();
}

QModelIndex
TreeModel::index(int row, int column, const QModelIndex &parent_index) const
{
	if (!hasIndex(row, column, parent_index)) {
		return QModelIndex();
	}
	
	TreeData &data = app_->tree_data();
	
	if (!parent_index.isValid())
	{
		TreeItem *next = data.roots[row];
		return createIndex(row, column, next);
	}
	
	TreeItem *parent = static_cast<TreeItem*>(parent_index.internalPointer());
	TreeItem *child = parent->child(row);
	if (child) {
		return createIndex(row, column, child);
	}
	
	return QModelIndex();
}

bool TreeModel::InsertBookmarkAt(const i32 at, TreeItem *item)
{
	TreeData &data = app_->tree_data();
	TreeItem *bookmarks_root = data.GetBookmarksRoot();
	CHECK_PTR(bookmarks_root);
	auto first = at;
	auto last = at;
	beginInsertRows(QModelIndex(), first, last);
	{
		bookmarks_root->AppendChild(item);
	}
	endInsertRows();
	
	return true;
}

void TreeModel::InsertFromAnotherThread(cornus::gui::InsertArgs args)
{
	static bool set_size_once = true;
	
	if (set_size_once) {
		if (app_->prefs().splitter_sizes().size() > 0)
			set_size_once = false;
	}
	
	if (set_size_once) {
		set_size_once = false;
		auto *splitter = app_->main_splitter();
		QFont font = view_->option().font;
		font.setBold(true);
		QFontMetrics fm(font);
		int widest = 0;
		for (TreeItem *next: args.partitions) {
			int n = fm.boundingRect(next->DisplayString()).width();
			if (n > widest) {
				widest = n;
			}
		}
		
		widest += 14;
		splitter->setSizes({widest, 1000});
	}
	
	InsertBookmarks(args.bookmarks);
	InsertPartitions(args.partitions);
	app_->tree_data().BroadcastPartitionsLoaded();
}

void TreeModel::InsertBookmarks(const QVector<cornus::gui::TreeItem*> &bookmarks)
{
	TreeData &data = app_->tree_data();
	int root_row;
	TreeItem *root = data.GetBookmarksRoot(&root_row);
	CHECK_PTR_VOID(root);
	{
		auto first = 0;
		auto last = first + bookmarks.size() - 1;
		auto root_index = createIndex(root_row, 0, root);
		beginInsertRows(root_index, first, last);
		{
			for (auto *item: bookmarks) {
				root->AppendChild(item);
			}
		}
		endInsertRows();
	}
}

bool TreeModel::InsertPartition(TreeItem *item)
{
	CHECK_TRUE(item->is_partition());
	PartitionInfo *info = item->partition_info();
	int root_row = -1;
	int first = -1;
	TreeItem *root = GetRootTS(info->disk_info, &root_row);
	CHECK_PTR(root);
	{
		first = -1;
		const int count = root->child_count();
		const QString s = item->toString();
		for (int i = 0; i < count; i++)
		{
			TreeItem *child = root->children()[i];
			if (s < child->toString()) {
				first = i;
				break;
			}
		}
		if (first == -1)
			first = count;
	}
	int last = first;
	auto root_index = createIndex(root_row, 0, root);
	beginInsertRows(root_index, first, last);
	{
		root->InsertChild(item, first);
	}
	endInsertRows();
	
	return true;
}

void TreeModel::InsertPartitions(const QVector<cornus::gui::TreeItem*> &partitions)
{
	const i32 count = partitions.size();
	
	for (i32 i = 0; i < count; i++) {
		TreeItem *next = partitions[i];
		InsertPartition(next);
	}
}

void TreeModel::MountEvent(const QString &path, const QString &fs_uuid,
	const PartitionEventType evt)
{
// NB: fs_uuid is always null on PartitionEventType::Unmount
	
	PendingCommands &cmds = view_->pending_commands();
	QModelIndex index;
	bool found = false;
	QString go_to, leave;
	TreeData &data = app_->tree_data();
	{
		if (evt == PartitionEventType::Unmount)
		{
			for (TreeItem *root: data.roots)
			{
				if (!root->is_disk())
					continue;
				int i = -1;
				for (TreeItem *child: root->children())
				{
					i++;
					if (child->mounted() && child->mount_path() == path) {
						if (cmds.ContainsPath(path, true)) {
							leave = path;
						}
						child->mounted(false);
						child->mount_path(QString());
						index = createIndex(i, 0, root);
						found = true;
						break;
					}
				}
				
				if (found)
					break;
			}
		} else if (evt == PartitionEventType::Mount) {
			for (TreeItem *root: data.roots)
			{
				if (!root->is_disk())
					continue;
				int i = -1;
				for (TreeItem *child: root->children())
				{
					i++;
					if (child->partition_info()->fs_uuid == fs_uuid) {
						if (cmds.ContainsFsUUID(fs_uuid, true)) {
							go_to = path;
						}
						child->mounted(true);
						child->mount_path(path);
						index = createIndex(i, 0, root);
						found = true;
						break;
					}
				}
				
				if (found)
					break;
			}
		}
	}
	
	if (found) {
		UpdateIndex(index);
	}
	
	cmds.RemoveExpired();
	
	if (!go_to.isEmpty()) {
		app_->GoToSimple(go_to);
	} else if (!leave.isEmpty()) {
		QString dir = app_->current_dir();
		if (dir.startsWith(path))
			app_->GoToSimple(QDir::homePath());
	}
}

void TreeModel::MoveBookmarks(QStringList str_list, const QPoint &pos)
{
	CHECK_TRUE_VOID((str_list.size() == 2));
	QModelIndex target;
	int drop_at = GetDropLocation(pos, target);
	if (drop_at == -1)
		return;
	
	const QString &name = str_list[0];
	const QString &path = str_list[1];
	TreeData &data = app_->tree_data();
	TreeItem *source_item = nullptr;
	int source_at = -1;
	int root_row = -1;
	TreeItem *bkm_root = data.GetBookmarksRoot(&root_row);
	CHECK_PTR_VOID(bkm_root);
	QModelIndex bkm_root_index = createIndex(root_row, 0, bkm_root);
	auto &vec = bkm_root->children();
	
	for (TreeItem *next: vec)
	{
		source_at++;
		if (next->bookmark_name() == name && next->bookmark_path() == path)
		{
			source_item = vec[source_at];
			break;
		}
	}
	
	CHECK_PTR_VOID(source_item);
	
	beginRemoveRows(bkm_root_index, source_at, source_at);
	{
		bkm_root->children().removeAt(source_at);
	}
	endRemoveRows();
	
	const int k = (source_at < drop_at) ? drop_at - 1 : drop_at;
	//mtl_info("source_at: %d, drop_at: %d, k: %d", source_at, drop_at, k);
	
	beginInsertRows(bkm_root_index, k, k);
	{
		bkm_root->children().insert(k, source_item);
	}
	endInsertRows();
	
	QItemSelectionModel *selection = view_->selectionModel();
	QModelIndex sel_index = createIndex(k, 0, source_item);
	const auto flags = QItemSelectionModel::Select | QItemSelectionModel::Rows;
	selection->clearSelection();
	selection->select(sel_index, flags);
	const auto curr_flags = QItemSelectionModel::Current | QItemSelectionModel::Rows;
	selection->setCurrentIndex(sel_index, curr_flags);
	
	app_->SaveBookmarks();
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid()) {
		mtl_trace("INVALID parent row: %d", index.row());
		return QModelIndex();
	}
	
	TreeItem *node = static_cast<TreeItem*>(index.internalPointer());
	TreeItem *parent = node->parent();
	
	if (parent == nullptr) {
		return QModelIndex();
	}
	
	return createIndex(parent->Row(), 0, parent);
}

int TreeModel::rowCount(const QModelIndex &parent_index) const
{
	TreeData &data = app_->tree_data();
	
	if (!parent_index.isValid()) {
		return data.roots.size();
	}
	
	TreeItem *parent = static_cast<TreeItem*>(parent_index.internalPointer());
	return parent->child_count();
}

void TreeModel::SetView(TreeView *p)
{
	view_ = p;
}

void TreeModel::SetupModelData()
{
	TreeData &data = app_->tree_data();
	beginInsertRows(QModelIndex(), 0, 0);
	{
		data.roots.append(TreeItem::NewBookmarksRoot(data.roots.size()));
	}
	endInsertRows();
}

void TreeModel::UpdateIndices(const QModelIndex &top_left,
	const QModelIndex &bottom_right)
{
	Q_EMIT dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}


void TreeModel::UpdateIndex(const QModelIndex &index)
{
	Q_EMIT dataChanged(index, index, {Qt::DisplayRole});
	//view_->update(index);
}

void TreeModel::UpdateVisibleArea()
{
	TreeData &data = app_->tree_data();
	if (data.roots.isEmpty())
		return;
	int len = data.roots.size();
	QModelIndex top_left = createIndex(0, 0, data.roots[0]);
	QModelIndex bottom_right = createIndex(len - 1, 0, data.roots[len - 1]);
	
	Q_EMIT dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

}
