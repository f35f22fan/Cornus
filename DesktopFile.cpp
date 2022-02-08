#include "DesktopFile.hpp"

#include "ByteArray.hpp"
#include "io/io.hh"

#include <QProcess>
#include <QRegularExpression>

namespace cornus {
static const QString DesktopExt = QLatin1String(".desktop");
///mpv: Categories=AudioVideo;Audio;Video;Player;TV;
///Totem: Categories=GTK;GNOME;AudioVideo;Player;Video;
///VLC: Categories=AudioVideo;Player;Recorder;

///clementine: Categories=AudioVideo;Player;Qt;Audio;
///rhythmbox: Categories=GNOME;GTK;AudioVideo;
///         : Categories=GNOME;GTK;AudioVideo;Audio;Player;

///Gwenview: Categories=Qt;KDE;Graphics;Viewer;Photography;
///Shotwell: Categories=Graphics;Photography;GNOME;GTK;
///          Categories=Graphics;Viewer;Photography;GNOME;GTK;

///Categories=GNOME;GTK;Utility;TextEditor;
///Categories=Qt;KDE;Utility;TextEditor;

namespace str {
static const QString Categories = QStringLiteral("Categories");
static const QString MimeType = QStringLiteral("MimeType");
static const QString NotShowIn = QStringLiteral("NotShowIn");
static const QString OnlyShowIn = QStringLiteral("OnlyShowIn");
}

bool ContainsDesktopFile(QVector<DesktopFile*> &vec,
	DesktopFile *p, int *ret_index)
{
	const QString id = p->GetId(); // not a getter function
	int i = 0;
	for (DesktopFile *next: vec) {
		if (next->type() == p->type() && next->GetId() == id) {
			if (ret_index != nullptr)
				*ret_index = i;
			return true;
		}
		i++;
	}
	
	return false;
}

Category GetToolkitFor(const Category desktop)
{
	switch(desktop) {
	case Category::KDE: return Category::Qt;
	case Category::Gnome: return Category::Gtk;
	case Category::Ubuntu: return Category::Gtk;
	case Category::Unity: return Category::Gtk;
	case Category::Xfce: return Category::Gtk;
	default: return Category::Qt;
	}
}

const QRegularExpression regex("\\$\\{?\\w+\\}?");
const auto MainGroupName = QLatin1String("Desktop Entry");
const auto KeyExec = QLatin1String("Exec");

namespace desktopfile {

Group::Group(const QString &name, DesktopFile *parent)
	: name_(name), parent_(parent) {}
Group::~Group() {}

Group*
Group::Clone(DesktopFile *new_parent) const
{
	Group *p = new Group(name_, new_parent);
	p->mimetypes_ = mimetypes_;
	p->kv_map_ = kv_map_;
	p->categories_ = categories_;
	p->only_show_in_ = only_show_in_;
	p->not_show_in_ = not_show_in_;
	
	return p;
}

QString Group::ExpandEnvVars(QString s, QProcessEnvironment &my_env)
{
	QRegularExpressionMatch match;
	while (true)
	{
		int index = s.indexOf(regex, 0, &match);
		if (index == -1)
			break;
		
		QString var_name = match.captured();
		//mtl_printq2("var_name initial: ", var_name);
		const int initial_var_len = var_name.size();
		if (var_name.at(1) == QChar('{') && var_name.endsWith('}'))
		{
			var_name = var_name.mid(2, initial_var_len - 3); // 3 = "{$}"
		} else {
			var_name = var_name.mid(1);
		}
		//mtl_printq2("var_name: ", var_name);
		//mtl_printq2("value: ", my_env.value(var_name));
		QString new_val = s.mid(0, index);
		QString var_value = my_env.value(var_name);
		if (var_value.isEmpty())
		{
			var_value = kv_map_.value(var_name);
			//mtl_printq2("var_value was empty, now: ", var_value);
			if (var_value.isEmpty() && parent_ != nullptr) {
				var_value = parent_->kv().value(var_name);
			}
		}
		new_val += var_value;
		new_val += s.mid(index + initial_var_len);
		s = new_val;
	}
	
	//mtl_printq2("Returning: ", s);
	return s;
}

Group*
Group::From(ByteArray &ba, DesktopFile *parent)
{
	Group *p = new Group(ba.next_string(), parent);
	
	{
		const u8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->only_show_in_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		const u8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->not_show_in_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		const u8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->categories_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		const i32 count = ba.next_i32();
		for (i32 i = 0; i < count; i++)
		{
			QString s = ba.next_string();
			p->mimetypes_.append(s);
		}
	}
	{
		const i32 count = ba.next_i32();
		for (i32 i = 0; i < count; i++)
		{
			QString key = ba.next_string();
			QString val = ba.next_string();
			p->kv_map_.insert(key, val);
		}
	}
	
	return p;
}

QString
Group::Get(const QString &key, QProcessEnvironment &env)
{
	QString val = value(key);
	if (val.isEmpty())
		return val;

	return ExpandEnvVars(val, env);
}

bool Group::IsMain() const {
	return name_ == MainGroupName;
}

void Group::Launch(const QString &working_dir, const QString &full_path)
{
	QString exec = value(KeyExec);
	if (exec.isEmpty())
		return;

	QProcessEnvironment custom_env = QProcessEnvironment::systemEnvironment();
	QStringList args = exec.split(' ', Qt::SkipEmptyParts);
	const QString env_key = QLatin1String("env");
	const int arg_count = args.size();
	QStringList app_args;
// Exec=env APPMENU_DISPLAY_BOTH=abc dolphin -caption "%c" %i --f${APPMENU_DISPLAY_BOTH}older=$HOME/.special
	for (int i = 0; i < arg_count; i++)
	{
		const QString &next = args[i];
		
		if (next == env_key)
		{
			if ((++i) >= arg_count)
				continue;
			
			auto env_pair = args[i].split('=');
			if (env_pair.size() == 2) {
				//mtl_info("Env vars: \"%s\"=\"%s\"", qPrintable(env_pair[0]), qPrintable(env_pair[1]));
				custom_env.insert(env_pair[0], env_pair[1]);
			}
		} else if (next.startsWith('%')) {
			if (next == QLatin1String("%f") || next == QLatin1String("%F"))
			{
				if (full_path.isEmpty())
					continue;
				app_args.append(full_path);
			} else if (next == QLatin1String("%u") || next == QLatin1String("%U")) {
				if (full_path.isEmpty())
					continue;
				auto url_str = QUrl::fromLocalFile(full_path).toString();
				app_args.append(url_str);
			}
		} else {
			QString s = ExpandEnvVars(next, custom_env);
			app_args.append(s);
		}
	}
	
//	for (auto &next: app_args) {
//		mtl_printq2("App arg: ", next);
//	}
	
	QString work_dir = working_dir;
	if (work_dir.isEmpty())
	{
// Path description in .desktop spec:
// If entry is of type Application, the working directory to run the program in.
		work_dir = GetPath(custom_env);
	}
	
	QString exe_str = app_args.takeFirst();
	QProcess *process = new QProcess();
	process->setProgram(exe_str);
	process->setArguments(app_args);
	process->setWorkingDirectory(work_dir);
	process->setProcessEnvironment(custom_env);
	process->startDetached();
	
///"/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=ghb --file-forwarding fr.handbrake.ghb @@ %f @@"
}

void Group::ListKV()
{
	QMapIterator<QString, QString> it(kv_map_);
	mtl_info("kv_map size: %d",  kv_map_.size());
	while (it.hasNext())
	{
		it.next();
		auto key = it.key().toLocal8Bit();
		auto val = it.value().toLocal8Bit();
		mtl_info("\"%s\": \"%s\"", key.data(), val.data());
	}
}

void Group::ParseLine(const QStringRef &line,
	const QHash<QString, Category> &possible_categories)
{
	int sep = line.indexOf('=');
	if (sep == -1)
		return;
	
	QString key = line.mid(0, sep).trimmed().toString();
	QString value = line.mid(sep + 1).trimmed().toString();
	
	if (key.isEmpty())
		return;
	
	kv_map_.insert(key, value);
	
	if (mimetypes_.isEmpty() && key == str::MimeType) {
		mimetypes_ = value.split(';', Qt::SkipEmptyParts);
	} else if (key == str::Categories) {
		auto categories = value.splitRef(';', Qt::SkipEmptyParts);
		for (QStringRef &next: categories)
		{
			Category c = possible_categories.value(next.toString().toLower(), Category::None);
			if (c != Category::None)
				categories_.append(c);
		}
	} else if (key == str::OnlyShowIn || key == str::NotShowIn) {
		const bool only_show_in = key == str::OnlyShowIn;
		auto categories = value.splitRef(';', Qt::SkipEmptyParts);
		for (QStringRef &next: categories)
		{
			Category c = possible_categories.value(next.toString().toLower(), Category::None);
			if (c != Category::None) {
				if (only_show_in)
					only_show_in_.append(c);
				else
					not_show_in_.append(c);
			}
		}
	}
}

Priority Group::Supports(const QString &mime, const MimeInfo info,
	const Category desktop) const
{
	if (not_show_in_.contains(desktop))
		return Priority::Ignore;
	
	if (!only_show_in_.isEmpty() && !only_show_in_.contains(desktop))
		return Priority::Ignore;
	
	const Category toolkit = GetToolkitFor(desktop);
	const bool has_tk = categories_.contains(toolkit);
	
	if (info != MimeInfo::None)
	{
		if (info == MimeInfo::Text && is_text_editor())
			return has_tk ? Priority::High : Priority::Normal;
		if (info == MimeInfo::Image && is_image_viewer())
			return has_tk ? Priority::High : Priority::Normal;
		if (info == MimeInfo::Audio && is_audio_player())
			return has_tk ? Priority::High : Priority::Normal;
		if (info == MimeInfo::Video && is_video_player())
			return has_tk ? Priority::High : Priority::Normal;
	}
	
	if (!mimetypes_.contains(mime))
		return Priority::Ignore;
	
	return has_tk ? Priority::Normal : Priority::Low;
}

void Group::WriteTo(ByteArray &ba)
{
	ba.add_string(name_);
	ba.add_u8((u8)only_show_in_.size());
	for (const auto &next: only_show_in_)
		ba.add_u8(static_cast<u8>(next));
	
	ba.add_u8((u8)not_show_in_.size());
	for (const auto &next: not_show_in_)
		ba.add_u8(static_cast<u8>(next));
	
	ba.add_u8((u8)categories_.size());
	for (const Category &next: categories_) {
		ba.add_u8(static_cast<u8>(next));
	}
	
	ba.add_i32(mimetypes_.size());
	for (auto &mime: mimetypes_)
	{
		ba.add_string(mime);
	}
	ba.add_i32(kv_map_.size());
	QMapIterator<QString, QString> it(kv_map_);
	while (it.hasNext())
	{
		it.next();
		ba.add_string(it.key());
		ba.add_string(it.value());
	}
}

} // namespace

DesktopFile::DesktopFile() {}

DesktopFile::~DesktopFile() {
	
	auto it = groups_.constBegin();
	while (it != groups_.constEnd()) {
		delete it.value();
		it++;
	}
	
	groups_.clear();
}

DesktopFile*
DesktopFile::Clone() const
{
	DesktopFile *new_desktop_file = new DesktopFile();
	new_desktop_file->type_ = type_;
	new_desktop_file->priority_ = priority_;
	
	if (is_desktop_file()) {
		new_desktop_file->name_ = name_;
		new_desktop_file->full_path_ = full_path_;
		
		QMapIterator<QString, desktopfile::Group*> it(groups_);
		while (it.hasNext()) {
			it.next();
			desktopfile::Group *group = it.value()->Clone(new_desktop_file);
			if (group->IsMain())
				new_desktop_file->main_group_ = group;
			new_desktop_file->groups_.insert(it.key(), group);
		}
	} else {
		new_desktop_file->full_path_ = full_path_;
	}
	
	return new_desktop_file;
}

QIcon
DesktopFile::CreateQIcon(QProcessEnvironment &env)
{
	QString s = GetIcon(env);
	if (s.isEmpty())
		return QIcon::fromTheme(QLatin1String("application-x-executable"));
	
	if (s.startsWith('/'))
		return QIcon(s);
	else
		return QIcon::fromTheme(s);
}

DesktopFile*
DesktopFile::From(ByteArray &ba)
{
	DesktopFile *new_desktop_file = new DesktopFile();
	new_desktop_file->type_ = (Type) ba.next_i8();
	new_desktop_file->priority_ = (Priority) ba.next_i8();
	
	if (new_desktop_file->is_desktop_file())
	{
		new_desktop_file->full_path_ = ba.next_string();
		new_desktop_file->name_ = ba.next_string();
		const i32 group_count = ba.next_i32();
		
		for (i32 i = 0; i < group_count; i++) {
			desktopfile::Group *group = desktopfile::Group::From(ba, new_desktop_file);
			if (group == nullptr)
				continue;
			
			new_desktop_file->groups_.insert(group->name(), group);
			if (group->IsMain())
				new_desktop_file->main_group_ = group;
		}
	} else if (new_desktop_file->is_just_exe_path()) {
		new_desktop_file->full_path_ = ba.next_string();
	} else {
		mtl_trace();
	}
	
	return new_desktop_file;
}

DesktopFile*
DesktopFile::FromPath(const QString &full_path, const QHash<QString, Category> &h)
{
	DesktopFile *p = new DesktopFile();
	
	if (p->Init(full_path, h))
		return p;

	delete p;
	return nullptr;
}

DesktopFile*
DesktopFile::JustExePath(const QString &full_path)
{
	auto *p = new DesktopFile();
	p->type_ = Type::JustExePath;
	p->full_path_ = full_path;
	return p;
}

MimeInfo
DesktopFile::GetForMime(const QString &mime)
{
	if (mime.startsWith(QLatin1String("text/")) || mime == QLatin1String("application/x-desktop"))
		return MimeInfo::Text;
	if (mime.startsWith(QLatin1String("image/")))
		return MimeInfo::Image;
	if (mime.startsWith(QLatin1String("audio/")))
		return MimeInfo::Audio;
	if (mime.startsWith(QLatin1String("video/")))
		return MimeInfo::Video;
	return MimeInfo::None;
}

QString
DesktopFile::GetId() const
{
	if (is_just_exe_path())
		return full_path_;
	
	if (id_cached_.isEmpty())
	{
		const QString root = QLatin1String("applications/");
		int index = full_path_.indexOf(root);
		if (index == -1) {
			mtl_trace();
			return QString();
		}
		
		auto id = full_path_.mid(index + root.size());
		id_cached_ = id.replace('/', '-');
	}
	
	return id_cached_;
}

QString
DesktopFile::GetIcon(QProcessEnvironment &env) const
{
	if (!is_desktop_file() || !main_group_)
		return QString();
	
	return main_group_->GetIcon(env);
}

QString
DesktopFile::GetGenericName() const {
	if (is_desktop_file() && main_group_)
		return main_group_->value(QLatin1String("GenericName"));
	
	return QString();
}

QString
DesktopFile::GetName() const
{
	if (is_just_exe_path())
		return io::GetFileNameOfFullPath(full_path_).toString();
	
	if (main_group_)
		return main_group_->value(QLatin1String("Name"));
	
	return QString();
}

bool DesktopFile::Init(const QString &full_path,
	const QHash<QString, Category> &possible_categories)
{
	type_ = Type::DesktopFile;
	full_path_ = full_path;
	possible_categories_ = &possible_categories;
	auto ref = io::GetFileNameOfFullPath(full_path_);
	if (!ref.endsWith(DesktopExt))
		return false;
	
	name_ = ref.mid(0, ref.size() - DesktopExt.size()).toString();
	MTL_CHECK(!name_.isEmpty());
	
	io::ReadParams param = {};
	ByteArray ba;
	MTL_CHECK(io::ReadFile(full_path, ba, param));
	
	QString text = QString::fromLocal8Bit(ba.data(), ba.size());
	QVector<QStringRef> list = text.splitRef('\n', Qt::SkipEmptyParts);
	desktopfile::Group *group = nullptr;
	
	for (const auto &next: list)
	{
		auto line = next.trimmed();
		if (line.startsWith('#') || line.isEmpty())
			continue;
		
		if (line.startsWith('['))
		{
			const int end = line.lastIndexOf(']');
			MTL_CHECK(end != -1);
			QStringRef name = line.mid(1, end - 1);
			group = new desktopfile::Group(name.toString(), this);
			if (main_group_ == nullptr && name == MainGroupName)
				main_group_ = group;
			
			continue;
		}
		
		if (group == nullptr)
		{
			const int sep = line.indexOf('=');
			if (sep == -1)
				continue;
			auto key = line.mid(0, sep).trimmed();
			auto value = line.mid(sep + 1).trimmed();
			
			if (key.isEmpty())
				continue;
			
			kv_.insert(key.toString(), value.toString());
		} else {
			group->ParseLine(line, possible_categories);
			groups_.insert(group->name(), group);
		}
	}
	
	return true;
}

bool DesktopFile::IsApp() const
{
	if (!main_group_)
		return false;
	
	return main_group_->value(QLatin1String("Type")) == QLatin1String("Application");
}

void DesktopFile::Launch(const DesktopArgs &desk_args)
{
	if (is_desktop_file())
	{
		if (main_group_ != nullptr)
			main_group_->Launch(desk_args.working_dir, desk_args.full_path);
	} else if (is_just_exe_path()) {
		QStringList args;
		args.append(desk_args.full_path);
		QProcess::startDetached(full_path_, args, desk_args.working_dir);
	} else {
		mtl_trace();
	}
}

bool DesktopFile::Reload()
{
	main_group_ = nullptr;
	type_ = Type::None;
	name_.clear();
	id_cached_.clear();
	
	auto it = groups_.constBegin();
	while (it != groups_.constEnd()) {
		delete it.value();
		it++;
	}
	groups_.clear();
	
	if (possible_categories_ == nullptr)
	{
		mtl_trace();
		return false;
	}
	
	return Init(full_path_, *possible_categories_);
}

Priority DesktopFile::Supports(const QString &mime, const MimeInfo info,
	const Category desktop) const
{
	if (!main_group_)
		return Priority::Ignore;
	
	if (desktop != Category::None && ToBeRunInTerminal())
		return Priority::Ignore;
	
	return main_group_->Supports(mime, info, desktop);
}

bool DesktopFile::ToBeRunInTerminal() const
{
	if (!is_desktop_file())
		return false;
	
	if (main_group_ == nullptr)
		return false;
	
	return main_group_->value(QLatin1String("Terminal")).toLower()
		== QLatin1String("true");
}

void DesktopFile::WriteTo(ByteArray &ba) const
{
	ba.add_i8(i8(type_));
	ba.add_i8(i8(priority_));
	
	if (is_desktop_file()) {
		ba.add_string(full_path_);
		ba.add_string(name_);
		ba.add_i32(groups_.size());
		
		for (desktopfile::Group *group: groups_) {
			group->WriteTo(ba);
		}
	} else {
		ba.add_string(full_path_);
	}
}

}
