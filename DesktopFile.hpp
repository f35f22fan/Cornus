#pragma once

#include <QIcon>
#include <QMap>
#include <QProcessEnvironment>

#include "category.hh"
#include "decl.hxx"
#include "err.hpp"

namespace cornus {

struct DesktopArgs {
	QString working_dir;
	QString full_path;
};

enum class MimeInfo: u8 {
	None,
	Audio,
	Video,
	Image,
	Text,
};

enum class Priority: i8 {
	Ignore = -1,
	Highest = 4,
	High = 3,
	Normal = 2,
	Low = 1,
};

namespace desktopfile {

class Group {
	
public:
	Group(const QString &name, DesktopFile *parent);
	~Group();
	
	Group* Clone(DesktopFile *new_parent) const;
	QString ExpandEnvVars(QString s, QProcessEnvironment &my_env);
	static Group* From(ByteArray &ba, DesktopFile *parent);
	QString Get(const QString &key, QProcessEnvironment &env);
	QString GetIcon(QProcessEnvironment &env) { return Get(QLatin1String("Icon"), env); }
	QString GetPath(QProcessEnvironment &env) { return Get(QLatin1String("Path"), env); }
	bool IsMain() const;
	void Launch(const QString &working_dir, const QString &full_path);
	const QString& name() const { return name_; }
	void ParseLine(const QStringRef &line, const QHash<QString, Category> &possible_categories);
	QMap<QString, QString>& map() { return kv_map_; }
	Priority Supports(const QString &mime, const MimeInfo info, const Category desktop) const;
	void WriteTo(ByteArray &ba);
	QString value(const QString &key) const { return kv_map_.value(key); }
	void ListKV();
	
	const QVector<Category>& categories() const { return categories_; }
	
	bool for_gnome() const { return categories_.contains(Category::Gnome); }
	bool for_kde() const { return categories_.contains(Category::KDE); }
	bool is_image_viewer() const { return categories_.contains(Category::Photography)
		&& categories_.contains(Category::Viewer); }
	bool is_text_editor() const { return categories_.contains(Category::TextEditor); }
	bool is_audio_player() const { return categories_.contains(Category::Player) &&
		categories_.contains(Category::Audio); }
	bool is_video_player() const { return categories_.contains(Category::Player) &&
		categories_.contains(Category::Video); }
	
private:
	NO_ASSIGN_COPY_MOVE(Group);
	QString name_;
	QMap<QString, QString> kv_map_;
	QStringList mimetypes_;
	QVector<Category> categories_;
	QVector<Category> only_show_in_;
	QVector<Category> not_show_in_;
	DesktopFile *parent_ = nullptr;
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

	virtual ~DesktopFile();
	
	static DesktopFile* From(ByteArray &ba);
	static DesktopFile* FromPath(const QString &full_path, const QHash<QString, Category> &h);
	static DesktopFile* JustExePath(const QString &full_path);
	DesktopFile* Clone() const;
	
	static MimeInfo GetForMime(const QString &mime);
	QIcon CreateQIcon(QProcessEnvironment &env);
	
	const QString& full_path() const { return full_path_; }
	void full_path(const QString &s) { full_path_ = s; }
	
	QString GetId() const;
	bool IsApp() const;
	bool IsTextEditor() const {
		return main_group_ != nullptr &&
		main_group_->is_text_editor();
	}
	
	const QHash<QString, QString>& kv() const { return kv_; }
	
	void Launch(const DesktopArgs &args);
	desktopfile::Group* main_group() const { return main_group_; }
	
	bool ToBeRunInTerminal() const;
	bool is_desktop_file() const { return type_ == Type::DesktopFile; }
	bool is_just_exe_path() const { return type_ == Type::JustExePath; }
	
	bool Reload();
	Priority Supports(const QString &mime, const MimeInfo info,
		const Category desktop) const;
	Type type() const { return type_; }
	void WriteTo(ByteArray &ba) const;
	
	QString GetIcon(QProcessEnvironment &env) const;
	QString GetName() const;
	QString GetGenericName() const;
	
	const QString& name() const { return name_; }
	
	Priority priority() const { return priority_; }
	void priority(const Priority n) { priority_ = n; }
	
private:
	NO_ASSIGN_COPY_MOVE(DesktopFile);
	DesktopFile();
	
	bool Init(const QString &full_path, const QHash<QString, Category> &possible_categories);
	
	Type type_ = Type::None;
	Priority priority_ = Priority::Ignore;
	QString full_path_;
	QString name_;
	mutable QString id_cached_;
	QMap<QString, desktopfile::Group*> groups_;
	desktopfile::Group *main_group_ = nullptr;
	const QHash<QString, Category> *possible_categories_ = nullptr;
	QHash<QString, QString> kv_;
};

bool ContainsDesktopFile(QVector<DesktopFile*> &vec, DesktopFile *p, int *ret_index = nullptr);

} /// namespace
