#include "DesktopFile.hpp"

#include "ByteArray.hpp"
#include "io/io.hh"

#include <QProcess>

namespace cornus {

const auto MainGroupName = QLatin1String("Desktop Entry");
const auto Exec = QLatin1String("Exec");

namespace desktopfile {
Group::Group(const QString &name): name_(name)
{}
Group::~Group() {
}

Group*
Group::From(ByteArray &ba)
{
	Group *p = new Group(ba.next_string());
	i32 mime_count = ba.next_i32();
	auto name = p->name().toLocal8Bit();
//	mtl_info("mime_count: %d, group: %s", mime_count, name.data());
	
	for (i32 i = 0; i < mime_count; i++) {
		QString s = ba.next_string();
		p->mimetypes_.append(s);
	}
	
	i32 kv_count = ba.next_i32();
	
	for (i32 i = 0; i < kv_count; i++) {
		QString key = ba.next_string();
		QString val = ba.next_string();
		p->kv_map_.insert(key, val);
	}
	
	return p;
}

void
Group::Launch(const QString &full_path, const QString &working_dir)
{
	
	QString exec = value(Exec);
	if (exec.isEmpty())
		return;
	QStringList args = exec.split(' ', Qt::SkipEmptyParts);
	
	for (auto &next: args) {
		if (next.startsWith('%')) {
			if (next == QLatin1String("%f") || next == QLatin1String("%F")) {
				next = full_path;
			} else if (next == QLatin1String("%u") || next == QLatin1String("%U")) {
				next = QUrl::fromLocalFile(full_path).toString();
			}
		}
	}
	
	QString exe = args.takeFirst();
	QProcess::startDetached(exe, args, working_dir);
	
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

void Group::WriteTo(ByteArray &ba)
{
	ba.add_string(name_);
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

void Group::ParseLine(const QStringRef &line)
{
	int sep = line.indexOf('=');
	if (sep == -1)
		return;
	
	QString key = line.mid(0, sep).trimmed().toString();
	QString value = line.mid(sep + 1).trimmed().toString();
	
	if (key.isEmpty())
		return;
	
	kv_map_.insert(key, value);
	
	if (mimetypes_.isEmpty() && key == QLatin1String("MimeType")) {
		mimetypes_ = value.split(';', Qt::SkipEmptyParts);
	}
}

bool Group::SupportsMime(const QString &mime)
{
	return mimetypes_.contains(mime);
}

} // namespace

DesktopFile::DesktopFile() {}

DesktopFile::~DesktopFile() {}

DesktopFile*
DesktopFile::From(ByteArray &ba)
{
	DesktopFile *p = new DesktopFile();
	p->full_path_ = ba.next_string();
	p->name_ = ba.next_string();
///	mtl_printq2("Desktop File name: ", p->name_);
	const i32 group_count = ba.next_i32();
	
	for (i32 i = 0; i < group_count; i++) {
		desktopfile::Group *group = desktopfile::Group::From(ba);
		if (group == nullptr)
			continue;
		
		p->groups_.insert(group->name(), group);
		
		if (group->name() == MainGroupName) {
			auto ba = group->name().toLocal8Bit();
			auto ba2 = p->name_.toLocal8Bit();
///			mtl_info("SET AS MAIN GROUP: %s for %s", ba.data(), ba2.data());
			p->main_group_ = group;
		}
	}
	
	return p;
}

DesktopFile*
DesktopFile::FromPath(const QString &full_path)
{
	DesktopFile *p = new DesktopFile();
	
	if (p->Init(full_path))
		return p;

	delete p;
	return nullptr;
}

QString
DesktopFile::GetIcon() const
{
	if (!main_group_)
		return QString();
	
	return main_group_->value(QLatin1String("Icon"));
}

QString
DesktopFile::GetName() const
{
	if (!main_group_)
		return QString();
	
	return main_group_->value(QLatin1String("Name"));
}

bool DesktopFile::Init(const QString &full_path)
{
	full_path_ = full_path;
	auto ref = io::GetFileNameOfFullPath(full_path_);
	int index = ref.lastIndexOf(QLatin1String(".desktop"));
	if (index == -1)
		return false;
	name_ = ref.mid(0, index).toString();
	
	ByteArray ba;
	if (io::ReadFile(full_path, ba) != io::Err::Ok)
		return false;
	
	QString text = QString::fromLocal8Bit(ba.data(), ba.size());
	QVector<QStringRef> list = text.splitRef('\n', Qt::SkipEmptyParts);
	desktopfile::Group *group = nullptr;
	
	for (const auto &next: list)
	{
		auto line = next.trimmed();
		if (line.startsWith('#') || line.isEmpty())
			continue;
		
		if (line.startsWith('[')) {
			int end = line.lastIndexOf(']');
			CHECK_TRUE((end != -1));
			QStringRef name = line.mid(1, end - 1);
			group = new desktopfile::Group(name.toString());
			if (main_group_ == nullptr && name == MainGroupName)
				main_group_ = group;
			
			continue;
		}
		
		CHECK_PTR(group);
		group->ParseLine(line);
		groups_.insert(group->name(), group);
	}
	
	return true;
}

void DesktopFile::LaunchByMainGroup(const QString &full_path, const QString &working_dir)
{
	if (main_group_ != nullptr)
		main_group_->Launch(full_path, working_dir);
}

bool
DesktopFile::SupportsMime(const QString &mime)
{
	if (!main_group_)
		return false;
	
	return main_group_->SupportsMime(mime);
}

void DesktopFile::WriteTo(ByteArray &ba)
{
	ba.add_string(full_path_);
	ba.add_string(name_);
	ba.add_i32(groups_.size());
	
	for (desktopfile::Group *group: groups_) {
		group->WriteTo(ba);
	}
}

}

