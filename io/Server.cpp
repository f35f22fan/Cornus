#include "Server.hpp"

#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../prefs.hh"
#include "../str.hxx"
#include "../gui/TasksWin.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

#include <bits/stdc++.h> /// std::sort()
#include <sys/epoll.h>

namespace cornus::io {

MutexGuard DesktopFiles::guard() const
{
	return MutexGuard(&mutex);
}

const size_t kInotifyEventBufLen = 8 * (sizeof(struct inotify_event) + 16);

struct DesktopFileWatchArgs {
	QStringList dir_paths;
	cornus::io::Server *server = nullptr;
};

void ReadEvent(int inotify_fd, char *buf,
	bool &has_been_unmounted_or_deleted, io::Server *server,
	QHash<int, QString> &fd_to_path)
{
	const ssize_t num_read = read(inotify_fd, buf, kInotifyEventBufLen);
	
	if (num_read <= 0)
	{
		if (num_read == -1)
			mtl_status(errno);
		return;
	}
	
	DesktopFiles &desktop_files = server->desktop_files();
	ssize_t add = 0;
	const QString desktop = QLatin1String(".desktop");
	
	for (char *p = buf; p < buf + num_read; p += add)
	{
		struct inotify_event *ev = (struct inotify_event*) p;
		QString dir_path = fd_to_path.value(ev->wd).toLocal8Bit();
		add = sizeof(struct inotify_event) + ev->len;
		const auto mask = ev->mask;
		const bool is_dir = mask & IN_ISDIR;
		
		if (mask & IN_CREATE) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			DesktopFile *p = DesktopFile::FromPath(full_path, server->possible_categories());
			if (p != nullptr)
			{
				auto guard = desktop_files.guard();
#ifdef CORNUS_DEBUG_SERVER_INOTIFY
				auto ba = full_path.toLocal8Bit();
				mtl_info("Created: %s", ba.data());
#endif
				desktop_files.hash.insert(p->GetId(), p);
			}
		} else if (mask & IN_DELETE) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			for (auto it = desktop_files.hash.constBegin();
				it != desktop_files.hash.constEnd(); it++)
			{
				DesktopFile *p = it.value();
				if (p->full_path() == full_path) {
					desktop_files.hash.erase(it);
#ifdef CORNUS_DEBUG_SERVER_INOTIFY
					auto ba = full_path.toLocal8Bit();
					mtl_info("Deleted %s", ba.data());
#endif
					break;
				}
			}
		} else if (mask & IN_DELETE_SELF) {
			mtl_warn("IN_DELETE_SELF");
			has_been_unmounted_or_deleted = true;
			break;
		} else if (mask & IN_MOVE_SELF) {
			mtl_warn("IN_MOVE_SELF");
		} else if (mask & IN_MOVED_FROM) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			for (auto it = desktop_files.hash.constBegin();
				it != desktop_files.hash.constEnd(); it++)
			{
				DesktopFile *p = it.value();
				if (p->full_path() == full_path) {
					desktop_files.hash.erase(it);
#ifdef CORNUS_DEBUG_SERVER_INOTIFY
					auto ba = full_path.toLocal8Bit();
					mtl_info("IN_MOVED_FROM(Deleted) %s", ba.data());
#endif
					break;
				}
			}
		} else if (mask & IN_MOVED_TO) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			DesktopFile *p = DesktopFile::FromPath(full_path, server->possible_categories());
			if (p != nullptr)
			{
				auto guard = desktop_files.guard();
#ifdef CORNUS_DEBUG_SERVER_INOTIFY
				auto ba = full_path.toLocal8Bit();
				mtl_info("IN_MOVED_TO (Created): %s", ba.data());
#endif
				desktop_files.hash.insert(p->GetId(), p);
			}
		} else if (mask & IN_Q_OVERFLOW) {
			mtl_warn("IN_Q_OVERFLOW");
		} else if (mask & IN_UNMOUNT) {
			has_been_unmounted_or_deleted = true;
			break;
		} else if (mask & IN_CLOSE_WRITE) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			for (auto it = desktop_files.hash.constBegin();
				it != desktop_files.hash.constEnd(); it++)
			{
				DesktopFile *p = it.value();
				if (p->full_path() == full_path) {
					p->Reload();
#ifdef CORNUS_DEBUG_SERVER_INOTIFY
					auto ba = full_path.toLocal8Bit();
					mtl_info("Reloaded %s", ba.data());
#endif
					break;
				}
			}
		} else if (mask & IN_IGNORED) {
		} else {
			mtl_warn("Unhandled inotify event: %u", mask);
		}
	}
}

void* WatchDesktopFileDirs(void *void_args)
{
	pthread_detach(pthread_self());
	CHECK_PTR_NULL(void_args);
	
	QScopedPointer<DesktopFileWatchArgs> args((DesktopFileWatchArgs*) void_args);
	Server *server = args->server;
	
	char *buf = new char[kInotifyEventBufLen];
	AutoDeleteArr ad_arr(buf);
	Notify &notify = server->notify();
	
	auto event_types = IN_CREATE | IN_DELETE | IN_DELETE_SELF
		| IN_MOVE_SELF | IN_CLOSE_WRITE | IN_MOVE; /// IN_MODIFY
	QHash<int, QString> fd_to_path;
	for (const QString &next: args->dir_paths)
	{
		auto path = next.toLocal8Bit();
		int wd = inotify_add_watch(notify.fd, path.data(), event_types);
		
		if (wd == -1) {
			mtl_status(errno);
			return nullptr;
		}
		
		fd_to_path.insert(wd, next);
	}
	
	int epfd = epoll_create(1);
	
	if (epfd == -1) {
		mtl_status(errno);
		return 0;
	}
	
	struct epoll_event pev = {};
	pev.events = EPOLLIN;
	pev.data.fd = notify.fd;
	
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify.fd, &pev)) {
		mtl_status(errno);
		close(epfd);
		return nullptr;
	}
	
	struct epoll_event poll_event;
	const int seconds = 1 * 1000;
	
	while (true)
	{
		int event_count = epoll_wait(epfd, &poll_event, 1, seconds);
		bool has_been_unmounted_or_deleted = false;
		
		if (event_count > 0) {
			ReadEvent(poll_event.data.fd, buf,
				has_been_unmounted_or_deleted, server, fd_to_path);
		}
		
		if (has_been_unmounted_or_deleted) {
			break;
		}
	}
	
	if (close(epfd)) {
		mtl_status(errno);
	}
	
	return nullptr;
}

Server::Server()
{
	notify_.Init();
	io::InitEnvInfo(category_, search_icons_dirs_, xdg_data_dirs_, possible_categories_);
	tasks_win_ = new gui::TasksWin();
	LoadDesktopFiles();
	InitTrayIcon();
}

Server::~Server() {
	notify_.Close();
	delete tray_menu_;
}

void Server::CopyURLsToClipboard(ByteArray *ba)
{
	QMimeData *mime = new QMimeData();
	
	QList<QUrl> urls;
	while (ba->has_more()) {
		urls.append(QUrl(ba->next_string()));
	}
	
	mime->setUrls(urls);
	QApplication::clipboard()->setMimeData(mime);
}

void Server::CutURLsToClipboard(ByteArray *ba)
{
	QMimeData *mime = new QMimeData();
	
	QList<QUrl> urls;
	while (ba->has_more()) {
		urls.append(QUrl(ba->next_string()));
	}
	
	mime->setUrls(urls);
	
	QByteArray kde_mark;
	char c = '1';
	kde_mark.append(c);
	mime->setData(str::KdeCutMime, kde_mark);
	
	QApplication::clipboard()->setMimeData(mime);
}

void Server::GetPreferredOrder(QString mime,
	QVector<DesktopFile*> &add_vec,
	QVector<DesktopFile*> &hide_vec)
{
	QString filename = mime.replace('/', '-');
	QString mime_config_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!mime_config_dir.endsWith('/'))
		mime_config_dir.append('/');
	
	QString full_path = mime_config_dir + filename;
	ByteArray buf;
	if (!ReadFile(full_path, buf, PrintErrors::No))
		return;
	
	while (buf.has_more())
	{
		const Present present = (Present)buf.next_i8();
		const QString id = buf.next_string();
		DesktopFile *p = nullptr;
		
		if (id.startsWith('/')) {
			p = DesktopFile::JustExePath(id);
		} else {
			p = desktop_files_.hash.value(id, nullptr);
		}
		
		if (p == nullptr) {
			mtl_printq2("Desktop File not found for ", id);
			continue;
		}
		
		if (present == Present::Yes)
			add_vec.append(p);
		else
			hide_vec.append(p);
	}
}

void Server::InitTrayIcon()
{
	tray_icon_ = new QSystemTrayIcon();
	tray_icon_->setIcon(QIcon(cornus::AppIconPath));
	tray_icon_->setVisible(true);
	tray_icon_->setToolTip("cornus I/O daemon");
	connect(tray_icon_, &QSystemTrayIcon::activated, this, &Server::SysTrayClicked);
	
	tray_menu_ = new QMenu(tasks_win_);
	tray_icon_->setContextMenu(tray_menu_);
	
	{
		QAction *action = tray_menu_->addAction(tr("Hide"));
		action->setIcon(QIcon::fromTheme(QLatin1String("go-bottom")));
		connect(action, &QAction::triggered, [=] {SetTrayVisible(false);});
	}
	
	{
		QAction *action = tray_menu_->addAction(tr("Quit"));
		action->setIcon(QIcon::fromTheme(QLatin1String("application-exit")));
		connect(action, &QAction::triggered, [=] {
			io::socket::SendQuitSignalToServer();
		});
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
	
	pthread_t th;
	DesktopFileWatchArgs *args = new DesktopFileWatchArgs();
	args->server = this;
	args->dir_paths = watch_desktop_file_dirs_;
	
	int status = pthread_create(&th, NULL, io::WatchDesktopFileDirs, args);
	if (status != 0)
		mtl_status(status);
}

void Server::LoadDesktopFilesFrom(QString dir_path)
{
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	QVector<QString> names;
	if (io::ListFileNames(dir_path, names) != io::Err::Ok)
		return;
	
	watch_desktop_file_dirs_.append(dir_path);
///	mtl_printq2("Inotify for: ", dir_path);
	
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
		
		auto *p = DesktopFile::FromPath(full_path, possible_categories_);
		if (p != nullptr) {
			auto guard = desktop_files_.guard();
			desktop_files_.hash.insert(p->GetId(), p);
		}
	}
}

void Server::QuitGuiApp()
{
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
	mtl_info("Quitting GUI App");
#endif
	QCoreApplication::quit();
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
	mtl_info("Done.");
#endif
}

void Server::SendAllDesktopFiles(const int fd)
{
	ByteArray ba;
	{
		auto guard = desktop_files_.guard();
		auto it = desktop_files_.hash.constBegin();
		while (it != desktop_files_.hash.constEnd())
		{
			DesktopFile *p = it.value();
			p->WriteTo(ba);
			it++;
		}
	}
	
	ba.Send(fd);
}

void Server::SendDesktopFilesById(ByteArray *ba, const int fd)
{
	ByteArray reply;
	{
		auto guard = desktop_files_.guard();
		while (ba->has_more()) {
			QString id = ba->next_string();
			DesktopFile *p = desktop_files_.hash.value(id, nullptr);
			if (p == nullptr) {
				mtl_printq2("Not found by ID: ", id);
				continue;
			}
			
			reply.add_string(id);
			p->WriteTo(reply);
		}
	}
	
	reply.Send(fd);
}

void Server::SendDefaultDesktopFileForFullPath(ByteArray *ba, const int fd)
{
	QString full_path = ba->next_string();
	QMimeType mt = mime_db_.mimeTypeForFile(full_path);
	const QString mime = mt.name();
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	GetDesktopFilesForMime(mime, show_vec, hide_vec);
	
	ByteArray reply;
	if (!show_vec.isEmpty())
	{
		DesktopFile *p = show_vec[0];
		p->WriteTo(reply);
	}
	
	reply.Send(fd);
}

bool SortByPriority(DesktopFile *a, DesktopFile *b)
{
	if (a->priority() == b->priority())
		return false;
	return a->priority() < b->priority() ? false : true;
}

void Server::GetDesktopFilesForMime(const QString &mime,
	QVector<DesktopFile*> &show_vec, QVector<DesktopFile*> &hide_vec)
{
	const MimeInfo mime_info = DesktopFile::GetForMime(mime);
	GetPreferredOrder(mime, show_vec, hide_vec);
	//mtl_info("Show: %d, hide: %d", show_vec.size(), hide_vec.size());
	{
		auto guard = desktop_files_.guard();
		auto it = desktop_files_.hash.constBegin();
		while (it != desktop_files_.hash.constEnd())
		{
			DesktopFile *p = it.value();
			it++;
			Priority pr = p->Supports(mime, mime_info, category_);
			
			if (pr == Priority::Ignore)
				continue;
			
			if (!ContainsDesktopFile(show_vec, p->GetId(), p->type())
				&& !ContainsDesktopFile(hide_vec, p->GetId(), p->type()))
			{
				p->priority(pr);
				show_vec.append(p);
			}
		}
	}
	
	std::sort(show_vec.begin(), show_vec.end(), SortByPriority);
}

void Server::SendOpenWithList(QString mime, const int fd)
{
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	GetDesktopFilesForMime(mime, show_vec, hide_vec);
	
	ByteArray ba;
	for (DesktopFile *next: show_vec)
	{
		ba.add_i8((i8)Present::Yes);
		next->WriteTo(ba);
	}
	
	for (DesktopFile *next: hide_vec) {
		ba.add_i8((i8)Present::No);
		next->WriteTo(ba);
	}
	
	ba.Send(fd);
}

void Server::SysTrayClicked()
{
	static bool flag = true;
	tasks_win_->setVisible(flag);
	if (flag) {
		tasks_win_->raise();
	}
	flag = !flag;
}

void Server::SetTrayVisible(const bool yes)
{
	tray_icon_->setVisible(yes);
}

} // namespace



