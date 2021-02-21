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

void
Server::GetOrderPrefFor(QString mime, QVector<DesktopFile*> &add_vec,
	QVector<DesktopFile*> &remove_vec)
{
	QString filename = mime.replace('/', '-');
	QString save_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!save_dir.endsWith('/'))
		save_dir.append('/');
	
	QString full_path = save_dir + filename;
	ByteArray buf;
	
	if (ReadFile(full_path, buf) != io::Err::Ok) {
		return;
	}
	
	while (buf.has_more()) {
		const DesktopFile::Action action = (DesktopFile::Action)buf.next_i8();
		
		QString id = buf.next_string();
		DesktopFile *p = desktop_files_.value(id, nullptr);
		if (p == nullptr) {
			mtl_printq2("Desktop File not found for ", id);
			continue;
		}
		
		if (action == DesktopFile::Action::Add)
			add_vec.append(p);
		else
			remove_vec.append(p);
	}
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

void Server::SendAllDesktopFiles(const int fd)
{
	ByteArray ba;
	foreach (DesktopFile *p, desktop_files_)
	{
		p->WriteTo(ba);
	}
	
	ba.Send(fd);
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
	QVector<DesktopFile*> send_vec;
	QVector<DesktopFile*> remove_vec;
	GetOrderPrefFor(mime, send_vec, remove_vec);
	
	foreach (DesktopFile *p, desktop_files_)
	{
		if (!p->SupportsMime(mime))
			continue;
		if (DesktopFileIndex(send_vec, p->GetId(), p->type()) == -1) {
			if (DesktopFileIndex(remove_vec, p->GetId(), p->type()) == -1)
				send_vec.append(p);
		}
	}
	
	ByteArray ba;
	for (DesktopFile *next: send_vec) {
		ba.add_i8((i8)DesktopFile::Action::Add);
		next->WriteTo(ba);
	}
	
	for (DesktopFile *next: remove_vec) {
		ba.add_i8((i8)DesktopFile::Action::Remove);
		next->WriteTo(ba);
	}
	
	ba.Send(fd);
}
} // namespace


