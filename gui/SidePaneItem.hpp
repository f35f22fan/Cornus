#pragma once

#include "../io/decl.hxx"
#include "../err.hpp"

#include <udisks/udisks.h>

#include<QString>

namespace cornus::gui {
enum class SidePaneItemType: u8 {
	None = 0,
	Partition,
	Bookmark
};

class SidePaneItem {
	const u8 SelectedBit = 1u << 0;
	const u8 MountedBit = 1u << 2;
	const u8 HasBeenClickedBit = 1u << 3;
	const u8 RemovableBit = 1u << 4;
	const u8 MountChangedBit = 1u << 5;
	
public:
	SidePaneItem() {}
	~SidePaneItem() {}
	SidePaneItem* Clone();
	void Init();
	
	static SidePaneItem* NewBookmark(io::File &file);
	static SidePaneItem* NewPartition(GVolume *vol);
	static SidePaneItem* NewPartitionFromDevPath(const QString &dev_path);
	
	const QString& bookmark_name() const { return bookmark_name_; }
	void bookmark_name(const QString &s) { bookmark_name_ = s; }
	
	QString DisplayString();
	
	bool has_been_clicked() const { return bits_ & HasBeenClickedBit; }
	void has_been_clicked(const bool flag) {
		if (flag) bits_ |= HasBeenClickedBit;
		else bits_ &= ~HasBeenClickedBit;
	}
	
	bool is_bookmark() const { return type_ == SidePaneItemType::Bookmark; }
	bool is_partition() const { return type_ == SidePaneItemType::Partition; }
	
	const QString& dev_path() const { return dev_path_; }
	void dev_path(const QString &s) { dev_path_ = s;}
	QString GetPartitionName() const;
	
	const QString& fs() const { return fs_; }
	void fs(const QString &s) { fs_ = s; }
	
	bool mounted() const { return bits_ & MountedBit; }
	void mounted(const bool flag) {
		if (flag) bits_ |= MountedBit;
		else bits_ &= ~MountedBit;
	}
	
	bool mount_changed() const { return bits_ & MountChangedBit; }
	void mount_changed(const bool flag) {
		if (flag) bits_ |= MountChangedBit;
		else bits_ &= ~MountChangedBit;
	}
	
	const QString& mount_path() const { return mount_path_; }
	void mount_path(const QString &s) { mount_path_ = s;}
	
	bool removable() const { return bits_ & RemovableBit; }
	void removable(const bool flag) {
		if (flag) bits_ |= RemovableBit;
		else bits_ &= ~RemovableBit;
	}
	
	bool selected() const { return bits_ & SelectedBit; }
	void selected(const bool flag) {
		if (flag) bits_ |= SelectedBit;
		else bits_ &= ~SelectedBit;
	}
	
	i64 size() const { return size_; }
	void size(i64 n) { size_ = n; }
	
	void type(SidePaneItemType t) { type_ = t; }
	SidePaneItemType type() const { return type_; }
	void set_partition() { type_ = gui::SidePaneItemType::Partition; }
	void set_bookmark() { type_ = gui::SidePaneItemType::Bookmark; }
private:
	
	QString dev_path_;
	QString bookmark_name_;
	QString mount_path_;
	QString fs_;
	i64 size_ = -1;
	QString size_str_;
	SidePaneItemType type_ = SidePaneItemType::None;
	u8 bits_ = 0;
};
}
