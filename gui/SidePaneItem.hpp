#pragma once

#include "../err.hpp"

#include<QString>

namespace cornus::gui {
enum class SidePaneItemType: u8 {
	None = 0,
	Partition,
	Bookmark
};

class SidePaneItem {
public:
	
	SidePaneItem* Clone();
	void Init();
	
	bool is_bookmark() const { return type_ == SidePaneItemType::Bookmark; }
	bool is_partition() const { return type_ == SidePaneItemType::Partition; }
	
	const QString& dev_path() const { return dev_path_; }
	void dev_path(const QString &s) { dev_path_ = s;}
	
	void fs(const QString &s) { fs_ = s; }
	
	const QString& mount_path() const { return mount_path_; }
	void mount_path(const QString &s) { mount_path_ = s;}
	
	bool selected() const { return selected_; }
	void selected(const bool flag) { selected_ = flag; }
	
	QString table_name() { return table_name_; }
	void table_name(const QString &s) { table_name_ = s; }
	
	void type(SidePaneItemType t) { type_ = t; }
	
private:
	
	void ReadSize();
	
	QString table_name_;
	QString dev_path_;
	QString mount_path_;
	QString fs_;
	i64 size_ = -1;
	QString size_str_;
	SidePaneItemType type_ = SidePaneItemType::None;
	bool selected_ = false;
};
}
