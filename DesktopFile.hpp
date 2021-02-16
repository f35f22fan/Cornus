#pragma once

#include <QMap>

#include "decl.hxx"
#include "err.hpp"

namespace cornus {

namespace desktopfile {
class Group {
public:
	Group(const QString &name);
	~Group();
	
	static Group* From(ByteArray &ba);
	const QString& name() { return name_; }
	void ParseLine(const QStringRef &line);
	QMap<QString, QString>& map() { return kv_map_; }
	bool SupportsMime(const QString &mime);
	void WriteTo(ByteArray &ba);
	QString value(const QString &key) const { return kv_map_.value(key); }
	void ListKV();
private:
	NO_ASSIGN_COPY_MOVE(Group);
	QString name_;
	QMap<QString, QString> kv_map_;
	QStringList mimetypes_;
	
	friend class DesktopFile;
};
} // namespace

class DesktopFile {
public:
	virtual ~DesktopFile();
	
	static DesktopFile* From(ByteArray &ba);
	static DesktopFile* FromPath(const QString &full_path);
	const QString& full_path() const { return full_path_; }
	void Launch(const QString &full_path, const QString working_dir);
	desktopfile::Group* main_group() const { return main_group_; }
	bool SupportsMime(const QString &mime);
	void WriteTo(ByteArray &ba);
	
	QString GetIcon() const;
	QString GetName() const;
	
	const QString& name() const { return name_; }
	
private:
	NO_ASSIGN_COPY_MOVE(DesktopFile);
	DesktopFile();
	
	bool Init(const QString &full_path);
	
	QString full_path_;
	QString name_;
	QMap<QString, desktopfile::Group*> groups_;
	desktopfile::Group *main_group_ = nullptr;
};
}
