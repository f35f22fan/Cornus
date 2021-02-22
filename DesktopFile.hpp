#pragma once

#include <QIcon>
#include <QMap>

#include "decl.hxx"
#include "err.hpp"

namespace cornus {

namespace desktopfile {
class Group {
public:
	Group(const QString &name);
	~Group();
	
	Group* Clone() const;
	static Group* From(ByteArray &ba);
	bool IsMain() const;
	void Launch(const QString &full_path, const QString &working_dir);
	const QString& name() const { return name_; }
	void ParseLine(const QStringRef &line);
	QMap<QString, QString>& map() { return kv_map_; }
	bool SupportsMime(const QString &mime) const;
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
	enum class Type: i8 {
		None = 0,
		DesktopFile,
		JustExePath,
	};
	
	enum class Action: i8 {
		None = 0,
		Add, /// used in prefs file to mark items to be added/removed
		Remove
	};

	virtual ~DesktopFile();
	
	static DesktopFile* From(ByteArray &ba);
	static DesktopFile* FromPath(const QString &full_path);
	static DesktopFile* JustExePath(const QString &full_path);
	DesktopFile* Clone() const;
	QIcon CreateQIcon();
	
	const QString& full_path() const { return full_path_; }
	void full_path(const QString &s) { full_path_ = s; }
	
	QString GetId() const;
	bool IsApp() const;
	void Launch(const QString &full_path, const QString &working_dir);
	desktopfile::Group* main_group() const { return main_group_; }
	
	bool is_desktop_file() const { return type_ == Type::DesktopFile; }
	bool is_just_exe_path() const { return type_ == Type::JustExePath; }
	
	bool Reload();
	bool SupportsMime(const QString &mime) const;
	Type type() const { return type_; }
	void WriteTo(ByteArray &ba) const;
	
	QString GetIcon() const;
	QString GetName() const;
	QString GetGenericName() const;
	
	const QString& name() const { return name_; }
	
private:
	NO_ASSIGN_COPY_MOVE(DesktopFile);
	DesktopFile();
	
	bool Init(const QString &full_path);
	
	Type type_ = Type::None;
	QString full_path_;
	QString name_;
	mutable QString id_cached_;
	QMap<QString, desktopfile::Group*> groups_;
	desktopfile::Group *main_group_ = nullptr;
};

int DesktopFileIndex(QVector<DesktopFile*> &vec, const QString &id,
	const DesktopFile::Type t);

} /// namespace
