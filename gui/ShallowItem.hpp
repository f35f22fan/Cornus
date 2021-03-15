#pragma once

#include "../err.hpp"

#include <QString>

namespace cornus::gui {
class ShallowItem {
public:
	ShallowItem();
	virtual ~ShallowItem();
	
	QString GetPartitionName() const;
	void Init();
	static void LoadAllPartitions(QVector<ShallowItem*> &vec);
	
	const QString& dev_path() const { return dev_path_; }
	void dev_path(const QString &s) { dev_path_ = s; }
	
	const QString& fs() const { return fs_; }
	void fs(const QString &s) { fs_ = s; }
	
	const QString& mount_path() const { return mount_path_; }
	void mount_path(const QString &s) { mount_path_ = s; }
	
	bool mounted() const { return mounted_; }
	void mounted(const bool b) { mounted_ = b; }
	
	bool removable() const { return removable_; }
	void removable(const bool b) { removable_ = b; }
	
	i64 size() const { return size_; }
	void size(const i64 n) { size_ = n; }

private:
	QString dev_path_;
	QString fs_;
	QString mount_path_;
	i64 size_ = -1;
	QString size_str_;
	bool mounted_ = false;
	bool removable_ = false;
};
}
