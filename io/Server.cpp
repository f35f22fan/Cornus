#include "Server.hpp"

#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../str.hxx"
#include "../gui/TasksWin.hpp"
#include "io.hh"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

namespace cornus::io {

Server::Server()
{
	io::SetupEnvSearchPaths(search_icons_dirs_, xdg_data_dirs_);
	tasks_win_ = new gui::TasksWin();
	
//	auto *p = DesktopFile::FromPath("/usr/share/applications/firefox.desktop");
//	delete p;
	
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
		if (p != nullptr)
			desktop_files_.append(p);
	}
}

void Server::SendOpenWithList(QString mime, int fd)
{
//	mtl_printq2("Received query for mime: ", mime);
	ByteArray ba;
	
	for (DesktopFile *next: desktop_files_) {
		if (next->SupportsMime(mime)) {
			next->WriteTo(ba);
		}
	}
	
	ba.Send(fd);
}
} // namespace


