#pragma once

#include "../io/decl.hxx"
#include "../err.hpp"

#include<QString>

namespace cornus::gui {
enum class SidePaneItemType: u8 {
	None = 0,
	Partition,
	Bookmark
};

namespace sidepaneitem {
const u8 SelectedBit = 1u << 0;
///const u8 MouseOverBit = 1u << 1;
const u8 MountedBit = 1u << 2;
const u8 EventInProgressBit = 1u << 3;
}

class SidePaneItem {
public:
	SidePaneItem() {}
	~SidePaneItem() {}
	SidePaneItem* Clone();
	void Init();
	
	static SidePaneItem* NewBookmark(io::File &file);
	
	const QString& bookmark_name() const { return bookmark_name_; }
	void bookmark_name(const QString &s) { bookmark_name_ = s; }
	
	QString DisplayString();
	
	bool event_in_progress() const { return bits_ & sidepaneitem::EventInProgressBit; }
	void event_in_progress(const bool flag) {
		if (flag)
			bits_ |= sidepaneitem::EventInProgressBit;
		else
			bits_ &= ~sidepaneitem::EventInProgressBit;
	}
	
	bool is_bookmark() const { return type_ == SidePaneItemType::Bookmark; }
	bool is_partition() const { return type_ == SidePaneItemType::Partition; }
	
	const QString& dev_path() const { return dev_path_; }
	void dev_path(const QString &s) { dev_path_ = s;}
	QString GetDevName() const;
	
	const QString& fs() const { return fs_; }
	void fs(const QString &s) { fs_ = s; }
	
	void major(const i64 n) { major_ = n; }
	i64 major() const { return major_; }
	
	void minor(const i64 n) { minor_ = n; }
	i64 minor() const { return minor_; }
	
	bool mounted() const { return bits_ & sidepaneitem::MountedBit; }
	void mounted(const bool flag) {
		if (flag)
			bits_ |= sidepaneitem::MountedBit;
		else
			bits_ &= ~sidepaneitem::MountedBit;
	}
	
	const QString& mount_path() const { return mount_path_; }
	void mount_path(const QString &s) { mount_path_ = s;}
	
//	bool mouse_over() const { return bits_ & sidepaneitem::MouseOverBit; }
//	void mouse_over(const bool flag) {
//		if (flag)
//			bits_ |= sidepaneitem::MouseOverBit;
//		else
//			bits_ &= ~sidepaneitem::MouseOverBit;
//	}
	
	bool selected() const { return bits_ & sidepaneitem::SelectedBit; }
	void selected(const bool flag) {
		if (flag)
			bits_ |= sidepaneitem::SelectedBit;
		else
			bits_ &= ~sidepaneitem::SelectedBit;
	}
	
	i64 size() const { return size_; }
	void size(i64 n) { size_ = n; }
	
	void type(SidePaneItemType t) { type_ = t; }
	SidePaneItemType type() const { return type_; }
private:
	
	void ReadStats();
	
	i64 major_ = -1;
	i64 minor_ = -1;
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
