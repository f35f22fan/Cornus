#include "TreeItem.hpp"

#include "../io/DirStream.hpp"
#include "../io/File.hpp"
#include "../io/io.hh"
#include "../ByteArray.hpp"
#include "sidepane.hh"

const i64 kBlockSize = 512;

namespace cornus::gui {

PartitionInfo* PartitionInfo::FromDevice(struct udev_device *dev)
{
	PartitionInfo *info = new PartitionInfo();
	info->dev_path = udev_device_get_devnode(dev);
	QString sz_str = udev_device_get_property_value(dev,
		"ID_PART_ENTRY_SIZE");
	
	bool ok;
	i64 size = sz_str.toLongLong(&ok);
	if (ok)
		info->size = size * kBlockSize;
	info->fs = udev_device_get_property_value(dev, "ID_FS_TYPE");
	info->uuid = udev_device_get_property_value(dev, "ID_PART_ENTRY_UUID");
	info->fs_uuid = udev_device_get_property_value(dev, "ID_FS_UUID");
	
//	printf("%s: %s, %s\n", qPrintable(info->dev_path),
//		qPrintable(info->uuid), qPrintable(info->fs_uuid));
	
	info->label = udev_device_get_property_value(dev, "ID_FS_LABEL");
	info->sys_path = udev_device_get_syspath(dev);
	
	dev_t devn = udev_device_get_devnum(dev);
	info->dev_num.major = major(devn);
	info->dev_num.minor = minor(devn);
	//mtl_info("dev_num: %d:%d", info->dev_num.major, info->dev_num.minor);
	
	return info;
}

TreeItem::~TreeItem()
{
	delete info_;
	qDeleteAll(children_);
}

TreeItem* TreeItem::FromDevice(struct udev_device *device,
	io::DiskInfo *disk_info)
{
	PartitionInfo *info = PartitionInfo::FromDevice(device);
	MTL_CHECK_ARG(info != nullptr, nullptr);
	
	TreeItem *item = new TreeItem();
	item->set_partition();
	item->partition_info(info);
	
	if (disk_info) {
		item->info_->disk_info = *disk_info;
	} else {
		auto *parent_device = udev_device_get_parent(device);
		MTL_CHECK_ARG(parent_device != nullptr, nullptr);
		sidepane::ReadDiskInfo(parent_device, item->info_->disk_info);
	}
	
	return item;
}

TreeItem*
TreeItem::NewBookmark(io::File &file)
{
	TreeItem *p = new TreeItem();
	p->type(TreeItemType::Bookmark);
	p->mount_path(file.is_dir_or_so() ? file.build_full_path()
		: file.dir_path());
	p->bookmark_name(file.name());
	return p;
}

TreeItem*
TreeItem::child(int row)
{
	if (row < 0 || row >= children_.size())
		return nullptr;
	return children_[row];
}

TreeItem*
TreeItem::Clone()
{
	TreeItem *p = new TreeItem();
	p->title_ = title_;
	p->mount_path_ = mount_path_;
	p->size_ = size_;
	p->type_ = type_;
	p->bits_ = bits_;
	if (info_) {
		p->info_ = info_->Clone();
	}
	
	for (TreeItem *next: children_) {
		p->children_.append(next->Clone());
	}
	
	return p;
}

void TreeItem::AppendChild(TreeItem *p) {
	p->parent_ = this;
	children_.append(p);
}

QVariant TreeItem::data(const int column) const
{
	auto *k = const_cast<TreeItem*>(this);
	return k->DisplayString();
}

bool TreeItem::DeleteChild(TreeItem *p)
{
	const int at = children_.indexOf(p);
	MTL_CHECK(at != -1);
	children_.remove(at);
	delete p;
	return true;
}

QString
TreeItem::DisplayString()
{
	if (is_bookmarks_root())
		return QObject::tr("Bookmarks");
	
	if (is_bookmark() || is_disk()) {
		return title_;
	}
	
	QString s;// QChar(0x2654) + ' ';
	
	const QString &dev_path = info_->dev_path;
	int index = dev_path.lastIndexOf('/');
	if (index == -1) {
		mtl_trace();
		return s;
	}
	
	s += dev_path.mid(index + 1) + ' ';
	
	if (!size_str_.isEmpty())
		s += size_str_ + ' ';
	
	QString size_str;
	if (info_->size > 0)
		size_str = io::SizeToString(info_->size, StringLength::Short);
	
	if (!info_->fs.isEmpty()) {
		s += '(';
		
		if (info_->size > 0) {
			s += size_str;
			s += QLatin1String(", ");
		}
		
		s += info_->fs;
		s += ')';
	} else if (info_->size > 0) {
		s += '(' + size_str + ')';
	}
	
	return s;
}

QString
TreeItem::GetPartitionName() const
{
	if (!info_)
		return QString();
	
	const QString &dev_path = info_->dev_path;
	int index = dev_path.lastIndexOf('/');
	if (index == -1) {
		mtl_trace();
		return QString();
	}
	
	return dev_path.mid(index + 1);
}

bool TreeItem::HasRootPartition() const
{
	if (!is_disk())
		return false;
	const QChar root_path = '/';
	for (TreeItem *next: children_)
	{
		if (next->mounted() && next->mount_path() == root_path)
			return true;
	}
	
	return false;
}

QModelIndex
TreeItem::IndexOf(const QModelIndex &index, TreeItem *p, const int col) const
{
	const int count = children_.size();
	for (int i = 0; i < count; i++) {
		TreeItem *next = children_[i];
		if (next == p)
			return index.sibling(i, col);
	}
	
	return QModelIndex();
}

void TreeItem::InsertChild(TreeItem *p, const int at)
{
	p->parent_ = this;
	const int count = children_.size();
	const int n = (at >= count) ? count : at;
	children_.insert(n, p);
}

TreeItem* TreeItem::NewBookmarksRoot(const int root_row)
{
	auto *p = new TreeItem();
	p->set_bookmarks_root();
	p->root_row(root_row);
	return p;
}

TreeItem* TreeItem::NewDisk(struct udev_device *device, const int root_row)
{
	auto *p = new TreeItem();
	const QString name = udev_device_get_property_value(device, "ID_MODEL");
	p->set_disk(name);
	p->root_row(root_row);
	
	sidepane::ReadDiskInfo(device, p->disk_info());
	
	return p;
}

TreeItem* TreeItem::NewDisk(const io::DiskInfo &disk_info, const int root_row)
{
	auto *p = new TreeItem();
	p->set_disk(disk_info.id_model);
	p->root_row(root_row);
	p->disk_info() = disk_info;
	
	return p;
}

int TreeItem::Row() const
{
	if (root_row_ != -1)
		return root_row_;
	
	if (!parent_)
		return 0;
	int r = parent_->children_.indexOf(const_cast<TreeItem*>(this));
	return r;
}

void TreeItem::set_disk(const QString &model)
{
	type_ = TreeItemType::Disk;
	title_ = model;
}

void TreeItem::set_bookmarks_root()
{
	type_ = TreeItemType::BookmarksRoot;
	title_ = QObject::tr("Bookmarks");
}

}
