#include "Server.hpp"

#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../prefs.hh"
#include "../str.hxx"
#include "../gui/TasksWin.hpp"
#include "io.hh"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

namespace cornus::io {

bool Contains(QVector<DesktopFile*> &vec, const QString &id,
	const DesktopFile::Type t)
{
	for (DesktopFile *p: vec) {
		if (p->type() == t && p->GetId() == id)
			return true;
	}
	
	return false;
}

Server::Server()
{
	io::SetupEnvSearchPaths(search_icons_dirs_, xdg_data_dirs_);
	tasks_win_ = new gui::TasksWin();
	LoadDesktopFiles();
	mtl_info("In total %d desktop files", desktop_files_.size());
}

Server::~Server() {}

void Server::CopyToClipboard(const QString &s)
{
	QMimeData *mime = new QMimeData();
	
	auto list = s.splitRef('\n');
	QList<QUrl> urls;
	for (auto &next: list) {
		urls.append(QUrl::fromLocalFile(next.toString()));
	}
	
	mime->setUrls(urls);
	QApplication::clipboard()->setMimeData(mime);
}

void Server::CutToClipboard(const QString &s)
{
	QMimeData *mime = new QMimeData();
	
	auto list = s.splitRef('\n');
	QList<QUrl> urls;
	for (auto &next: list) {
		urls.append(QUrl::fromLocalFile(next.toString()));
	}
	
	mime->setUrls(urls);
	
	QByteArray ba;
	char c = '1';
	ba.append(c);
	mime->setData(str::KdeCutMime, ba);
	
	QApplication::clipboard()->setMimeData(mime);
}

QVector<DesktopFile*>
Server::GetOrderPrefFor(QString mime)
{
	QVector<DesktopFile*> vec;
	QString filename = mime.replace('/', '-');
	QString save_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!save_dir.endsWith('/'))
		save_dir.append('/');
	
	QString full_path = save_dir + filename;
	ByteArray buf;
	if (ReadFile(full_path, buf) != io::Err::Ok) {
		return vec;
	}
	
	while (buf.has_more()) {
		DesktopFile::Type t = (DesktopFile::Type)buf.next_i8();
		QString id = buf.next_string();
		auto *p = desktop_files_.value(id, nullptr);
		if (p == nullptr) {
			mtl_printq2("Desktop File not found for ", id);
			continue;
		}
		
		vec.append(p);
	}
	
	return vec;
}

void Server::LoadDesktopFiles()
{
	for (const auto &next: xdg_data_dirs_) {
		QString dir = next;
		if (!dir.endsWith('/'))
			dir.append('/');
		dir.append(QLatin1String("applications/"));
		LoadDesktopFilesFrom(dir);
	}
}

void Server::LoadDesktopFilesFrom(QString dir_path)
{
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	QVector<QString> names;
	if (io::ListFileNames(dir_path, names) != io::Err::Ok)
		return;
	
	struct statx stx;
	const auto flags = 0;///AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	for (auto &name: names) {
		QString full_path = dir_path + name;
		auto ba = full_path.toLocal8Bit();
		if (statx(0, ba.data(), flags, fields, &stx) != 0)
			continue;
		
		if (S_ISDIR(stx.stx_mode)) {
			LoadDesktopFilesFrom(full_path);
			continue;
		}
		
		auto *p = DesktopFile::FromPath(full_path);
		if (p != nullptr) {
			desktop_files_.insert(p->GetId(), p);
		}
	}
}

void Server::SendDesktopFilesById(ByteArray *ba, const int fd)
{
	ByteArray reply;
	while (ba->has_more()) {
		QString id = ba->next_string();
		DesktopFile *p = desktop_files_.value(id, nullptr);
		if (p == nullptr) {
			mtl_printq2("Not found by ID: ", id);
			continue;
		}
		
		reply.add_string(id);
		p->WriteTo(reply);
	}
	
	reply.Send(fd);
}

void Server::SendOpenWithList(QString mime, const int fd)
{
	QVector<DesktopFile*> vec = GetOrderPrefFor(mime);
	
	foreach (DesktopFile *p, desktop_files_)
	{
		if (!p->SupportsMime(mime))
			continue;
		if (!Contains(vec, p->GetId(), p->type()))
			vec.append(p);
	}
	
	ByteArray ba;
	for (DesktopFile *next: vec) {
		next->WriteTo(ba);
	}
	
	ba.Send(fd);
}
} // namespace


