#pragma once

#include "../io/decl.hxx"
#include "../err.hpp"

#include <libudev.h>
#include <udisks/udisks.h>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>

namespace cornus::gui {
enum class TreeItemType: u1 {
	None = 0,
	Partition,
	Bookmark,
	Disk,
	BookmarksRoot,
};

struct PartitionInfo
{
// udevadm info --query=property /dev/sda3
	
	/// ID_FS_TYPE="vfat"
	QString fs;
	
/// ID_PART_ENTRY_UUID="fceef57f-868c-4091-b420-62457a686690"
	QString uuid; // uniquely identifies a GPT partition
	
/// ID_FS_UUID is the UUID reported by glib/gio/GMount
	QString fs_uuid; // uniquely identifies a filesystem
	// Changes if the partition is reformatted
	// If a particular filesystem does not support UUIDs, a shorter identifier is used.

/// ID_PART_ENTRY_SIZE="1048576"
	i8 size = -1;
	
/// DEVNAME="/dev/sda2"
	QString dev_path;
	
/// ID_FS_LABEL="Fedora"
	QString label;
	
	// Needed to create an udev_device:
	QString sys_path;
	
	io::DevNum dev_num = { -1, -1 };
	io::DiskInfo disk_info = {};
	
	PartitionInfo* Clone() const
	{
		auto *p = new PartitionInfo();
		p->fs = fs;
		p->uuid = uuid;
		p->fs_uuid = fs_uuid;
		p->size = size;
		p->dev_path = dev_path;
		p->label = label;
		p->sys_path = sys_path;
		p->dev_num = dev_num;
		p->disk_info = disk_info;
		
		return p;
	}
	
	static PartitionInfo* FromDevice(struct udev_device *dev);
};

using BitsType = u1;

enum class Bits: BitsType {
	Empty = 0,
	Mounted = 1u << 0,
	Removable = 1u << 1,
	MountChanged = 1u << 2,
	CurrentPartition = 1u << 3,
};

inline Bits operator | (Bits a, Bits b) {
	return static_cast<Bits>(static_cast<u1>(a) | static_cast<u1>(b));
}

inline Bits& operator |= (Bits &a, const Bits b) {
	a = a | b;
	return a;
}

inline Bits operator ~ (Bits a) {
	return static_cast<Bits>(~(static_cast<u1>(a)));
}

inline Bits operator & (Bits a, Bits b) {
	return static_cast<Bits>((static_cast<u1>(a) & static_cast<u1>(b)));
}

inline Bits& operator &= (Bits &a, const Bits b) {
	a = a & b;
	return a;
}

class TreeItem {

public:
	TreeItem() {}
	~TreeItem();
	TreeItem* Clone();
	
	static TreeItem* FromDevice(struct udev_device *device,
		io::DiskInfo *disk_info);
	static TreeItem* NewBookmark(io::File &file);
	static TreeItem* NewBookmarksRoot(const int root_row);
	static TreeItem* NewDisk(struct udev_device *device, const int root_row);
	static TreeItem* NewDisk(const io::DiskInfo &disk_info, const int root_row);
	const QString& bookmark_name() const { return title_; }
	void bookmark_name(const QString &s) { title_ = s; }
	const QString& bookmark_path() const { return mount_path_; }
	void bookmark_path(const QString &s) { mount_path_ = s; }
	
	QString DisplayString();
	void set_partition() { type_ = gui::TreeItemType::Partition; }
	PartitionInfo* partition_info() const { return info_; }
	void partition_info(PartitionInfo *info) {
		if (info_)
			delete info_;
		info_ = info;
		set_partition();
	}
	
	const QString dev_path() const { return info_ ? info_->dev_path : QString(); }
	
	bool current_partition() const { return (bits_ & Bits::CurrentPartition) != Bits::Empty; }
	void current_partition(const bool flag) {
		if (flag)
			bits_ |= Bits::CurrentPartition;
		else
			bits_ &= ~Bits::CurrentPartition;
	}
	
	bool HasRootPartition() const;
	
	bool is_bookmark() const { return type_ == TreeItemType::Bookmark; }
	bool is_partition() const { return type_ == TreeItemType::Partition; }
	bool is_bookmarks_root() const { return type_ == TreeItemType::BookmarksRoot; }
	bool is_disk() const { return type_ == TreeItemType::Disk; }
	void set_disk(const QString &model);
	void set_bookmarks_root();
	
	QString GetPartitionName() const;
	bool mounted() const { return (bits_ & Bits::Mounted) != Bits::Empty; }
	void mounted(const bool flag) {
		if (flag) bits_ |= Bits::Mounted;
		else bits_ &= ~Bits::Mounted;
	}
	
	bool mount_changed() const { return (bits_ & Bits::MountChanged) != Bits::Empty; }
	void mount_changed(const bool flag) {
		if (flag) bits_ |= Bits::MountChanged;
		else bits_ &= ~Bits::MountChanged;
	}
	
	const QString& mount_path() const { return mount_path_; }
	void mount_path(const QString &s) { mount_path_ = s;}
	
	bool removable() const { return (bits_ & Bits::Removable) != Bits::Empty; }
	void removable(const bool flag) {
		if (flag) bits_ |= Bits::Removable;
		else bits_ &= ~Bits::Removable;
	}
	
	void root_row(const int i) { root_row_ = i; }
	
	i8 size() const { return size_; }
	void size(i8 n) { size_ = n; }
	
	void type(TreeItemType t) { type_ = t; }
	TreeItemType type() const { return type_; }
	void set_bookmark() { type_ = gui::TreeItemType::Bookmark; }
	
	QModelIndex IndexOf(const QModelIndex &index, TreeItem *p, const int col = 0) const;
	
	void AppendChild(TreeItem *p);
	void InsertChild(TreeItem *p, const int place);
	TreeItem* child(int row);
	int child_count() const { return children_.size(); }
	QVector<TreeItem*>& children() { return children_; }
	bool DeleteChild(TreeItem *p);
	
	int column_count() const { return 1; }
	TreeItem* parent() const { return parent_; }
	QVariant data(const int column) const;
	int Row() const;
	io::DiskInfo& disk_info() { return disk_info_; }
	void disk_info(const io::DiskInfo &n) { disk_info_ = n; }
	
	QString toString() { return DisplayString(); }
private:
	
	QString title_;
	QString mount_path_;
	i8 size_ = -1;
	QString size_str_;
	PartitionInfo *info_ = nullptr;
	TreeItemType type_ = TreeItemType::None;
	int root_row_ = -1;
	Bits bits_ = Bits::Empty;
	io::DiskInfo disk_info_ = {};
	
	TreeItem *parent_ = nullptr;
	QVector<TreeItem*> children_;
};
}
Q_DECLARE_METATYPE(cornus::gui::TreeItem*);
