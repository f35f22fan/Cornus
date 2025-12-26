#include "DesktopFile.hpp"

#include "ByteArray.hpp"
#include "io/io.hh"

#include <QLocale>
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
const QRegularExpression VariableRegex("\\$\\{?\\w+\\}?");
const auto MainGroupName = QLatin1String("Desktop Entry");
const auto KeyExec = QLatin1String("Exec");

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

QStringList SplitIntoArgs(QString s)
{
	const QChar bs('\\');
	//mtl_printq2("BS (Y): ", s);
	const QChar quote('\"');
	QStringList args;
	int last_quote = -1;
	QString arg;
	bool last_was_ctrl_bs = false;
	for (int i = 0; i < s.size(); i++)
	{
		const QChar c = s.at(i);
		if (c == bs)
		{
			bool bs_behind = (i - 1 >= 0) && (s.at(i - 1) == bs);
			if (bs_behind && last_was_ctrl_bs) {
				arg.append(c);
				last_was_ctrl_bs = false;
			} else {
				last_was_ctrl_bs = true;
			}
			continue;
		}
		
		if (c == quote)
		{
			cbool last_was_bs = last_was_ctrl_bs && (i - 1 >= 0) && (s.at(i - 1) == bs);
			if (last_was_bs) {
				arg.append(c);
				continue;
			}
			
			last_quote = (last_quote == -1) ? i : -1;
			continue;
		}
		
		if (last_quote == -1 && (c == ' ' || c == '\t'))
		{
			if (!arg.isEmpty()) {
				args.append(arg);
				arg.clear();
			}
			
			continue;
		}
		
		arg.append(c);
	}
	
	if (!arg.isEmpty())
		args.append(arg);
	
//	for (auto &next: args) {
//		mtl_printq(next);
//	}
	
	return args;
}

namespace desktopfile {

Group::Group(const QString &name, DesktopFile *parent)
	: group_name_(name), parent_(parent) {}
Group::~Group() {}

void Group::AddLocaleKV(QString key, const QString value)
{
	const int index = key.indexOf('[');
	MTL_CHECK_VOID(index != -1);
	
	auto base = key.mid(0, index);
	const int at = index + 1;
	auto loc_str = key.mid(at, key.size() - at - 1);
	QLocale locale(loc_str);
	//mtl_info("\"%s\" - \"%s\"", qPrintable(base), qPrintable(loc.toString()));
	locale_strings_[base].append({locale, value});
}

Group* Group::Clone(DesktopFile *new_parent) const
{
	Group *p = new Group(group_name_, new_parent);
	p->mimetypes_ = mimetypes_;
	p->kv_ = kv_;
	p->categories_ = categories_;
	p->only_show_in_ = only_show_in_;
	p->not_show_in_ = not_show_in_;
	
	return p;
}

QString Group::ExpandEnvVars(QString s, const QHash<QString, QString> *primary)
{
	if (!parent_) {
		return QString();
	}
	
	QRegularExpressionMatch match;
	while (true)
	{
		const int variable_index = s.indexOf(VariableRegex, 0, &match);
		if (variable_index == -1)
			break;
		
		QString var_name = match.captured();
		const int initial_var_len = var_name.size();
		if (var_name.at(1) == QChar('{') && var_name.endsWith('}'))
		{
			var_name = var_name.mid(2, initial_var_len - 3); // 3 = "{$}"
		} else {
			var_name = var_name.mid(1);
		}

		QString expanded_value = s.mid(0, variable_index);
		QString var_value;
		if (primary != nullptr && primary->contains(var_name))
		{
			var_value = primary->value(var_name);
		}
		
		if (var_value.isEmpty())
		{
			var_value = parent_->custom_env_.value(var_name);
			if (var_value.isEmpty())
			{
				var_value = kv_.value(var_name);
				if (var_value.isEmpty())
					var_value = parent_->env_.value(var_name);
			}
		}
		
		expanded_value += var_value;
		expanded_value += s.mid(variable_index + initial_var_len);
		s = expanded_value;
	}
	
	return s;
}

Group* Group::From(ByteArray &ba, DesktopFile *parent)
{
	Group *p = new Group(ba.next_string(), parent);
	
	{
		cu8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->only_show_in_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		cu8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->not_show_in_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		cu8 count = ba.next_u8();
		for (u8 i = 0; i < count; i++)
			p->categories_.append(static_cast<Category>(ba.next_u8()));
	}
	{
		ci32 count = ba.next_i32();
		for (i32 i = 0; i < count; i++)
		{
			QString s = ba.next_string();
			p->mimetypes_.append(s);
		}
	}
	{
		ci32 count = ba.next_i32();
		for (i32 i = 0; i < count; i++)
		{
			QString key = ba.next_string();
			QString val = ba.next_string();
			p->kv_.insert(key, val);
		}
	}
	
	return p;
}

QString Group::Get(const QString &key)
{
	QString val = value(key);
	if (val.isEmpty())
		return val;

	return ExpandEnvVars(val);
}

bool Group::IsMain() const { return group_name_ == MainGroupName; }

void Group::Launch(const QString &working_dir, const QString &full_path)
{
	const QString exec = value(KeyExec);
	if (exec.isEmpty())
		return;
	
	QHash<QString,QString> primary;
	QStringList args = SplitIntoArgs(exec);
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
			if (env_pair.size() == 2)
			{
// mtl_info("Env vars: \"%s\"=\"%s\"", qPrintable(env_pair[0]), qPrintable(env_pair[1]));
				parent_->env_.insert(env_pair[0], env_pair[1]);
			}
		} else if (next.startsWith('%')) {
			if (next == QLatin1String("%f") || next == QLatin1String("%F"))
			{
// %f: a single file path, %F: a list of file paths
				if (full_path.isEmpty())
					continue;
				app_args.append(full_path);
			} else if (next == QLatin1String("%u") || next == QLatin1String("%U")) {
// %u: a single URL, %U: a list of URLs
				if (full_path.isEmpty())
				{
					continue;
				}
				auto url_str = QUrl::fromLocalFile(full_path).toString();
				app_args.append(url_str);
			} else if (next == QLatin1String("%c")) {
// %c: The translated name of the application as listed in the
// appropriate Name key in the desktop entry.
				auto s = parent_->GetName(QLocale::system());
				if (!s.isEmpty())
					app_args.append(s);
			} else if (next == QLatin1String("%i")) {
// %i: The Icon key of the desktop entry expanded as two arguments,
// first --icon and then the value of the Icon key. Should not expand to
// any arguments if the Icon key is empty or missing.
				auto s = parent_->GetIcon();
				if (!s.isEmpty())
				{
					app_args.append(QLatin1String("--icon"));
					app_args.append(s);
				}
			} else if (next == QLatin1String("%k")) {
// %k: The location of the desktop file as either a URI (if for example
// gotten from the vfolder system) or a local filename or empty if no
// location is known.
				auto url_str = QUrl::fromLocalFile(parent_->full_path_).toString();
				app_args.append(url_str);
			}
		} else {
			QString s = ExpandEnvVars(next, &primary);
			app_args.append(s);
		}
	}
	
// "Path" description in .desktop spec:
// If entry is of type Application, the working directory to run the program in.
	QString work_dir = GetPath();
	if (work_dir.isEmpty())
		work_dir = working_dir;
	
	QString exe_str = app_args.takeFirst();
	QProcess *process = new QProcess();
	process->setProgram(exe_str);
	process->setArguments(app_args);
	process->setWorkingDirectory(work_dir);
	process->setProcessEnvironment(parent_->env_);
	process->startDetached();
	
///"/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=ghb --file-forwarding fr.handbrake.ghb @@ %f @@"
}

void Group::ListKV()
{
	auto it = kv_.constBegin();
	mtl_info("kv_map size: %lld",  kv_.size());
	while (it != kv_.constEnd())
	{
		auto key = it.key().toLocal8Bit();
		auto val = it.value().toLocal8Bit();
		mtl_info("\"%s\": \"%s\"", key.data(), val.data());
		it++;
	}
}

void Group::ParseLine(QStringView line,
	const QHash<QString, Category> &possible_categories)
{
	cint sep = line.indexOf('=');
	if (sep == -1)
		return;
	
	QString key = line.mid(0, sep).trimmed().toString();
	if (key.isEmpty())
		return;
	
	QString value = line.mid(sep + 1).trimmed().toString();
	kv_.insert(key, value);
	
	if (key.endsWith(']'))
	{
		AddLocaleKV(key, value);
		return;
	}
	
	if (mimetypes_.isEmpty() && key == str::MimeType)
	{
		mimetypes_ = value.split(';', Qt::SkipEmptyParts);
		return;
	}
	
	if (key == str::Categories)
	{
		QStringList categories = value.split(';', Qt::SkipEmptyParts);
		for (QString &next: categories)
		{
			Category c = possible_categories.value(next.toLower(), Category::None);
			if (c != Category::None)
				categories_.append(c);
		}
		return;
	}
	cbool only_show_in = key == str::OnlyShowIn;
	if (only_show_in || key == str::NotShowIn)
	{
		QStringList categories = value.split(';', Qt::SkipEmptyParts);
		for (QString &next: categories)
		{
			const QString key = next.toLower();
			Category c = possible_categories.value(key, Category::None);
			if (c != Category::None)
			{
				if (only_show_in)
					only_show_in_.append(c);
				else
					not_show_in_.append(c);
			}
		}
	}
}

inline i16 Score(const QLocale &lhs, const QLocale &rhs)
{
	i16 score = 0;
	if (lhs.language() == rhs.language())
		score += 32;
	
	if (lhs.territory() == rhs.territory())
		score += 16;
	
	return score;
}

QString Group::PickByLocale(const QLocale &match_locale, const QString &key)
{
	if (locale_cache_.contains(match_locale))
	{
		auto &h = locale_cache_[match_locale];
		if (h.contains(key))
			return h.value(key);
	}
	
	if (!locale_strings_.contains(key))
		return QString();
	
	const QPair<QLocale, QString> *best_pair = nullptr;
	i16 best_score = -1;
	auto vec = locale_strings_.value(key);
	for (auto &pair: vec)
	{
		const int score = Score(pair.first, match_locale);
		if (score > best_score)
		{
			best_pair = &pair;
			best_score = score;
		}
	}
	
	if (best_pair == nullptr)
		return QString();
	
	{
		auto &h = locale_cache_[match_locale];
		h.insert(key, best_pair->second);
	}
	
	return best_pair->second;
}

Priority Group::Supports(const QString &mime, const MimeInfo info,
	const Category desktop) const
{
	if (not_show_in_.contains(desktop))
		return Priority::Ignore;
	
	if (!only_show_in_.isEmpty() && !only_show_in_.contains(desktop))
		return Priority::Ignore;
	
	const Category toolkit = GetToolkitFor(desktop);
	cbool has_tk = categories_.contains(toolkit);
	
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
	ba.add_string(group_name_);
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
	ba.add_i32(kv_.size());
	
	auto it = kv_.constBegin();
	while (it != kv_.constEnd())
	{
		ba.add_string(it.key());
		ba.add_string(it.value());
		it++;
	}
}

} // namespace

DesktopFile::DesktopFile(const QProcessEnvironment &env): env_(env){}

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
	DesktopFile *new_df = new DesktopFile(env_);
	new_df->type_ = type_;
	new_df->priority_ = priority_;
	new_df->custom_env_ = custom_env_;
	
	if (is_desktop_file())
	{
		new_df->name_ = name_;
		new_df->full_path_ = full_path_;
		
		auto it = groups_.constBegin();
		while (it != groups_.constEnd())
		{
			desktopfile::Group *group = it.value()->Clone(new_df);
			if (group->IsMain())
				new_df->main_group_ = group;
			new_df->groups_.insert(it.key(), group);
			it++;
		}
	} else {
		new_df->full_path_ = full_path_;
	}
	
	return new_df;
}

QIcon DesktopFile::CreateQIcon()
{
	QString s = GetIcon();
	if (s.isEmpty())
		return QIcon::fromTheme(QLatin1String("application-x-executable"));
	
	if (s.startsWith('/'))
		return QIcon(s);
	else
		return QIcon::fromTheme(s);
}

DesktopFile*
DesktopFile::From(ByteArray &ba, const QProcessEnvironment &env)
{
	DesktopFile *new_df = new DesktopFile(env);
	new_df->type_ = (Type) ba.next_i8();
	new_df->priority_ = (Priority) ba.next_i8();
	
	cu16 custom_env_count = ba.next_u16();
	for (u16 i = 0; i < custom_env_count; i++)
	{
		QString k = ba.next_string();
		QString v = ba.next_string();
		new_df->custom_env_.insert(k, v);
		new_df->env_.insert(k, v);
	}
	
	if (new_df->is_desktop_file())
	{
		new_df->full_path_ = ba.next_string();
		new_df->name_ = ba.next_string();
		const i32 group_count = ba.next_i32();
		
		for (i32 i = 0; i < group_count; i++) {
			auto *group = desktopfile::Group::From(ba, new_df);
			if (group == nullptr)
				continue;
			
			new_df->groups_.insert(group->name(), group);
			if (group->IsMain())
				new_df->main_group_ = group;
		}
	} else if (new_df->is_just_exe_path()) {
		new_df->full_path_ = ba.next_string();
	} else {
		mtl_trace();
	}
	
	return new_df;
}

DesktopFile*
DesktopFile::FromPath(const QString &full_path, const QHash<QString,
	Category> &h, const QProcessEnvironment &env)
{
	DesktopFile *p = new DesktopFile(env);
	
	if (p->Init(full_path, h))
		return p;

	delete p;
	return nullptr;
}

QString DesktopFile::GetComment(const QLocale &locale)
{
	if (is_desktop_file() && main_group_)
	{
		const QString key = QLatin1String("Comment");
		QString s = main_group_->PickByLocale(locale, key);
		return s.isEmpty() ? main_group_->value(key) : s;
	}
	
	return QString();
}

MimeInfo DesktopFile::GetForMime(const QString &mime)
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

QString DesktopFile::GetId() const
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

QString DesktopFile::GetIcon()
{
	if (!is_desktop_file() || !main_group_)
		return QString();
	
	return main_group_->GetIcon();
}

QString DesktopFile::GetGenericName(const QLocale &locale)
{
	if (is_desktop_file() && main_group_)
	{
		const QString key = QLatin1String("GenericName");
		QString s = main_group_->PickByLocale(locale, key);
		return s.isEmpty() ? main_group_->value(key) : s;
	}
	
	return QString();
}

QString DesktopFile::GetName(const QLocale &locale)
{
	if (is_just_exe_path())
		return io::GetFileNameOfFullPath(full_path_).toString();
	
	if (main_group_)
	{
		//QLocale lc(QLocale::Russian, QLocale::AnyScript, QLocale::Belarus);
		const QString key = QLatin1String("Name");
		QString s = main_group_->PickByLocale(locale, key);
		return s.isEmpty() ? main_group_->value(key) : s;
	}
	
	return QString();
}

// If entry is of type Application, the working directory to run the program in. 
QString DesktopFile::GetPath()
{
	if (!is_desktop_file() || !main_group_)
		return QString();
	
	return main_group_->GetPath();
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
	QStringList list = text.split('\n', Qt::SkipEmptyParts);
	desktopfile::Group *group = nullptr;
	
	for (const QString &next: list)
	{
		QString line = next.trimmed();
		if (line.startsWith('#') || line.isEmpty())
			continue;
		
		if (line.startsWith('['))
		{
			cint end = line.lastIndexOf(']');
			MTL_CHECK(end != -1);
			QString name = line.mid(1, end - 1);
			group = new desktopfile::Group(name, this);
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
			
			custom_env_.insert(key, value);
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

DesktopFile*
DesktopFile::JustExePath(const QString &full_path, const QProcessEnvironment &env)
{
	auto *p = new DesktopFile(env);
	p->type_ = Type::JustExePath;
	p->full_path_ = full_path;
	return p;
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
	
	cu16 count = custom_env_.count();
	ba.add_u16(count);
	auto it = custom_env_.constBegin();
	while (it != custom_env_.constEnd())
	{
		ba.add_string(it.key());
		ba.add_string(it.value());
		it++;
	}
	
	if (is_desktop_file())
	{
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
