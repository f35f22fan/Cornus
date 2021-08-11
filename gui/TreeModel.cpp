#include "TreeModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../Prefs.hpp"
#include "sidepane.hh"
#include "../TreeData.hpp"
#include "TreeItem.hpp"
#include "TreeView.hpp"

#include <QDir>
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
	}
	
	return QVariant();
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
			
			int at = -1;
			{
				auto g = data.guard();
				at = data.roots.size();
			}
			TreeItem *root = TreeItem::NewDisk(device, at);
			auto index = QModelIndex();
			beginInsertRows(index, at, at);
			{
				auto g = data.guard();
				data.roots.append(root);
			}
			endInsertRows();
		} else if (action == DeviceAction::Removed) {
			// Creating a udev_device from sys_path here will fail
			// because the device is removed.
			int i = -1;
			data.Lock();
			for (TreeItem *root: data.roots)
			{
				i++;
				if (root->is_disk() && root->disk_info().dev_path == dev_path) {
					data.Unlock();
					beginRemoveRows(QModelIndex(), i, i);
					{
						auto g = data.guard();
						data.roots.remove(i);
						delete root;
					}
					endRemoveRows();
					data.Lock();
					break;
				}
			}
			data.Unlock();
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
		InsertPartition(child, InsertPlace::Sorted);
	} else if (action == DeviceAction::Removed) {
		bool done = false;
		{
			auto g = data.guard();
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
							data.Unlock();
							beginRemoveRows(parent_index, i, i);
							{
								data.Lock();
								vec.remove(i);
								delete child;
								done = true;
								data.Unlock();
							}
							endRemoveRows();
							data.Lock();
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

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	
	return QAbstractItemModel::flags(index);
}

gui::TreeItem* TreeModel::GetRootTS(const io::DiskInfo &disk_info, int *row)
{
	TreeData &data = app_->tree_data();
	int count = -1;
	{
		auto g = data.guard();
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
		auto g = data.guard();
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
	auto g = data.guard();
	
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
	TreeItem *bookmarks_root = nullptr;
	{
		MutexGuard g = data.guard();
		bookmarks_root = data.GetBookmarksRootNTS();
	}
	CHECK_PTR(bookmarks_root);
	auto first = at;
	auto last = at;
	beginInsertRows(QModelIndex(), first, last);
	{
		auto g = data.guard();
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
	app_->tree_data().BroadcastPartitionsLoadedLTH();
}

void TreeModel::InsertBookmarks(const QVector<cornus::gui::TreeItem*> &bookmarks)
{
	TreeItem *root = nullptr;
	TreeData &data = app_->tree_data();
	int root_row;
	{
		MutexGuard g = data.guard();
		root = data.GetBookmarksRootNTS(&root_row);
	}
	CHECK_PTR_VOID(root);
	{
		auto first = 0;
		auto last = first + bookmarks.size() - 1;
		auto root_index = createIndex(root_row, 0, root);
		beginInsertRows(root_index, first, last);
		{
			MutexGuard g = data.guard();
			for (auto *item: bookmarks) {
				root->AppendChild(item);
			}
		}
		endInsertRows();
	}
}

bool TreeModel::InsertPartition(TreeItem *item, const InsertPlace place)
{
	CHECK_TRUE(item->is_partition());
	TreeData &data = app_->tree_data();
	
	PartitionInfo *info = item->partition_info();
	int root_row = -1;
	int first = -1;
	TreeItem *root = GetRootTS(info->disk_info, &root_row);
	{
		auto g = data.guard();
		CHECK_PTR(root);
		if (place == InsertPlace::AtEnd) {
			first = root->child_count();
		} else if (place == InsertPlace::AtStart) {
			return false;
		} else if (place == InsertPlace::Sorted) {
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
	}
	int last = first;
	auto root_index = createIndex(root_row, 0, root);
	beginInsertRows(root_index, first, last);
	{
		MutexGuard g = data.guard();
		root->InsertChild(item, first);
	}
	endInsertRows();
	
	return true;
}

void TreeModel::InsertPartitions(const QVector<cornus::gui::TreeItem*> &partitions)
{
	{
		const i32 count = partitions.size();
		
		for (i32 i = 0; i < count; i++) {
			TreeItem *next = partitions[i];
			InsertPartition(next, InsertPlace::Sorted);
		}
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
		auto g = data.guard();
		
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

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid()) {
		mtl_trace("INVALID parent row: %d", index.row());
		return QModelIndex();
	}
	
///	mtl_info("parent(): row: %d, col: %d", index.row(), index.column());
	
	TreeItem *node = static_cast<TreeItem*>(index.internalPointer());
	TreeItem *parent = node->parent();
	
	if (parent == nullptr) {
		return QModelIndex();
	}
	
	return createIndex(parent->Row(), 0, parent);
}

int TreeModel::rowCount(const QModelIndex &parent_index) const
{
	int c = rowCount_real(parent_index);
	return c;
}

int TreeModel::rowCount_real(const QModelIndex &parent_index) const
{
	if (!parent_index.isValid()) {
		TreeData &data = app_->tree_data();
		auto g = data.guard();
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
		auto guard = data.guard();
		data.roots.append(TreeItem::NewBookmarksRoot(data.roots.size()));
	}
	endInsertRows();
}

void TreeModel::UpdateIndex(const QModelIndex &index)
{
	///Q_EMIT dataChanged(index, index, {Qt::DisplayRole});
	view_->update(index);
}

void TreeModel::UpdateVisibleArea()
{
	QModelIndex top_left, bottom_right;
	int len = -1;
	{
		TreeData &data = app_->tree_data();
		auto g = data.guard();
		if (data.roots.isEmpty())
			return;
		len = data.roots.size();
		top_left = createIndex(0, 0, data.roots[0]);
		bottom_right = createIndex(len - 1, 0, data.roots[len - 1]);
	}
	
	Q_EMIT dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

}
