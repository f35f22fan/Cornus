#include "Daemon.hpp"

#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "DirStream.hpp"
#include "../prefs.hh"
#include "../str.hxx"
#include "../trash.hh"
#include "../gui/TasksWin.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>

#include <bits/stdc++.h> /// std::sort()
#include <sys/epoll.h>
#include <sys/eventfd.h>

namespace cornus::io {

const int OneHourInMs = 1000 * 60 * 60;

bool SortByPriority(DesktopFile *a, DesktopFile *b)
{
	if (a->priority() == b->priority())
		return false;
	return a->priority() < b->priority() ? false : true;
}

MutexGuard DesktopFiles::guard() const
{
	return MutexGuard(&mutex);
}

const size_t kInotifyEventBufLen = 16 * (sizeof(struct inotify_event) + NAME_MAX + 1);

struct DesktopFileWatchArgs {
	QStringList dir_paths;
	cornus::io::Daemon *server = nullptr;
};

void ReadEvent(int inotify_fd, char *buf,
	bool &has_been_unmounted_or_deleted, io::Daemon *daemon,
	QHash<int, QString> &fd_to_path)
{
	const ssize_t num_read = read(inotify_fd, buf, kInotifyEventBufLen);
	
	if (num_read <= 0)
	{
		if (num_read == -1)
			mtl_status(errno);
		return;
	}
	
	DesktopFiles &desktop_files = daemon->desktop_files();
	ssize_t add = 0;
	const QString desktop = QLatin1String(".desktop");
	
	for (char *p = buf; p < buf + num_read; p += add)
	{
		struct inotify_event *ev = (struct inotify_event*) p;
		QString dir_path = fd_to_path.value(ev->wd).toLocal8Bit();
		add = sizeof(struct inotify_event) + ev->len;
		const auto mask = ev->mask;
		cbool is_dir = mask & IN_ISDIR;
		
		if (mask & IN_CREATE) {
			QString name(ev->name);
			if (is_dir || !name.endsWith(desktop))
				continue;
			QString full_path = dir_path + name;
			DesktopFile *p = DesktopFile::FromPath(full_path,
				daemon->possible_categories(), daemon->env());
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
			DesktopFile *p = DesktopFile::FromPath(full_path,
				daemon->possible_categories(), daemon->env());
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
	if (!void_args) {
		return NULL;
	}
	
	QScopedPointer<DesktopFileWatchArgs> args((DesktopFileWatchArgs*) void_args);
	Daemon *server = args->server;
	CondMutex *cm = server->get_exit_cm();
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
		
		if (wd == -1)
		{
			mtl_warn("%s: \"%s\"", strerror(errno), path.data());
			return nullptr;
		}
		
		fd_to_path.insert(wd, next);
	}
	
	cint epfd = epoll_create(1);
	if (epfd == -1)
	{
		mtl_status(errno);
		return 0;
	}
	
	AutoCloseFd epoll_fd_(epfd);
	struct epoll_event pev = {};
	pev.events = EPOLLIN;
	pev.data.fd = notify.fd;
	cint quit_fd = server->signal_quit_fd();
	
	QVector<struct epoll_event> evt_vec(2);
	evt_vec[0].events = EPOLLIN;
	evt_vec[0].data.fd = notify.fd;
	evt_vec[1].events = EPOLLIN;
	evt_vec[1].data.fd = quit_fd;
	
	for (auto &next: evt_vec)
	{
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, next.data.fd, &next))
		{
			mtl_status(errno);
			return nullptr;
		}
	}
	
	struct epoll_event poll_event;
	cint seconds = 10 * 1000;
	while (true)
	{
		cint num_fds = epoll_wait(epfd, &poll_event, 1, seconds);
		cbool do_exit = cm->GetFlag(Lock::Yes);
		if (do_exit)
			break;
		bool has_been_unmounted_or_deleted = false;
		
		for (int i = 0; i < num_fds; i++)
		{
			cauto &evt = evt_vec[i];
			if (!(evt.events & EPOLLIN))
				continue;
			
			if (evt.data.fd == quit_fd)
			{
				io::ReadEventFd(evt.data.fd);
				return nullptr;
			}
			
			if (evt.data.fd == notify.fd)
			{
				ReadEvent(poll_event.data.fd, buf,
					has_been_unmounted_or_deleted, server, fd_to_path);
			}
			
		}
		
		if (has_been_unmounted_or_deleted) {
			break;
		}
	}
	
	return nullptr;
}

Daemon::Daemon()
{
	signal_quit_fd_ = ::eventfd(0, 0);
	if (signal_quit_fd_ == -1)
		mtl_status(errno);
	
	cm_ = new CondMutex();
	life_ = new ServerLife();
	notify_.Init();
	env_ = QProcessEnvironment::systemEnvironment();
	io::InitEnvInfo(category_, search_icons_dirs_, xdg_data_dirs_, possible_categories_, env_);
	tasks_win_ = new gui::TasksWin();
	LoadDesktopFiles();
	InitTrayIcon();
	QTimer::singleShot(OneHourInMs, this, &Daemon::CheckOldThumbnails);
}

Daemon::~Daemon() {
	{
		{ // wake up epoll() to not wait till it times out
			ci64 n = 1;
			const int status = ::write(signal_quit_fd_, &n, sizeof n);
			if (status == -1)
				mtl_status(errno);
		}
		::close(signal_quit_fd_);
	}
	notify_.Close();
	delete life_;
	life_ = nullptr;
	delete tray_menu_;
	delete tasks_win_;
	tasks_win_ = nullptr;
}

void Daemon::CheckOldThumbnails()
{
	const int three_days = OneHourInMs * 24 * 3;
	QTimer::singleShot(three_days, this, &Daemon::CheckOldThumbnails);
	
	const QString &dir_path = io::GetLastingTmpDir();
	MTL_CHECK_VOID(!dir_path.isEmpty());
	ci64 ten_days = 60 * 60 * 24 * 10;
	ci64 now_minus_ten_days = i64(time(NULL)) - ten_days;
	io::DirStream ds(dir_path);
	while (io::DirItem *next = ds.next())
	{
		ci64 modif_time_sec = next->stx.stx_mtime.tv_sec;
		if (modif_time_sec > now_minus_ten_days)
			continue;
		auto ba = (dir_path + next->name).toLocal8Bit();
		mtl_info("Removing %s", next->name);
		int status = ::remove(ba.data());
		if (status != 0)
			mtl_status(errno);
	}
}

bool Daemon::EmptyTrashRecursively(QString dp, const bool notify_user)
{
	if (!dp.endsWith('/'))
		dp.append('/');
	const QIcon trash_icon = QIcon::fromTheme(QLatin1String("user-trash"));
	QVector<QString> names;
	int status = io::ListDirNames(dp, names, ListDirOption::DontIncludeLinksToDir);
	if (status != 0)
	{
		QString err = strerror(status) + QLatin1String("<br/>") + dp;
		tray_icon_->showMessage(tr("Trash error"), err, trash_icon);
		return false;
	}
	
	if (names.isEmpty())
	{
		if (notify_user)
		{
			tray_icon_->showMessage(tr("There was nothing in the trash can"),
				dp, trash_icon);
		}
		return true;
	}
	
	const int index = names.indexOf(trash::name());
	
	if (index != -1) {
		const QString full_path = dp + trash::name();
		status = io::DeleteFolder(full_path);
		names.remove(index);
		if (status != 0) {
			QString err = strerror(status) + QLatin1String("<br/>") + full_path;
			tray_icon_->showMessage(tr("Trash error"), err, trash_icon);
			return false;
		}
	}
	
	for (const QString &name: names)
	{
		if (!EmptyTrashRecursively(dp + name, false))
			return false;
	}
	
	if (notify_user)
	{
		tray_icon_->showMessage(tr("Trash can is now empty"), dp, trash_icon);
	}
	
	return true;
}

void Daemon::GetDesktopFilesForMime(const QString &mime,
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
			
			if (ContainsDesktopFile(show_vec, p) || ContainsDesktopFile(hide_vec, p))
				continue;
			
			const Priority pr = p->Supports(mime, mime_info, category_);
			if (pr == Priority::Ignore)
				continue;
			
			p->priority(pr);
			show_vec.append(p);
		}
	}
	
	std::sort(show_vec.begin(), show_vec.end(), SortByPriority);
}

void Daemon::GetPreferredOrder(QString mime,
	QVector<DesktopFile*> &show_vec,
	QVector<DesktopFile*> &hide_vec)
{
	QString filename = mime.replace('/', '-');
	QString full_path = cornus::prefs::QueryMimeConfigDirPath();
	if (!full_path.endsWith('/'))
		full_path.append('/');
	
	full_path += filename;
	io::ReadParams rp = {};
	rp.print_errors = PrintErrors::No;
	rp.can_rely = CanRelyOnStatxSize::Yes;
	ByteArray buf;
	if (!ReadFile(full_path, buf, rp))
		return;
	
	while (buf.has_more())
	{
		const Present present = (Present)buf.next_i8();
		const QString id = buf.next_string();
		DesktopFile *p = nullptr;
		
		if (id.startsWith('/')) {
			p = DesktopFile::JustExePath(id, env_);
		} else {
			p = desktop_files_.hash.value(id, nullptr);
		}
		
		if (p == nullptr) {
			mtl_printq2("Desktop File not found for ", id);
			continue;
		}
		
		if (present == Present::Yes) {
			p->priority(Priority::Highest);
			show_vec.append(p);
		} else {
			p->priority(Priority::Ignore);
			hide_vec.append(p);
		}
	}
}

void Daemon::InitTrayIcon()
{
	tray_icon_ = new QSystemTrayIcon();
	tray_icon_->setIcon(QIcon(cornus::AppIconPath));
	tray_icon_->setVisible(true);
	tray_icon_->setToolTip("cornus I/O daemon");
	connect(tray_icon_, &QSystemTrayIcon::activated, this, &Daemon::SysTrayClicked);
	
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

void Daemon::LoadDesktopFiles()
{
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	for (const auto &next: xdg_data_dirs_)
	{
		QString dir = next;
		if (!dir.endsWith('/'))
			dir.append('/');
		dir.append(QLatin1String("applications/"));
		LoadDesktopFilesFrom(dir, env);
	}
	
	DesktopFileWatchArgs *args = new DesktopFileWatchArgs();
	args->server = this;
	args->dir_paths = watch_desktop_file_dirs_;
	
	io::NewThread(io::WatchDesktopFileDirs, args);
}

void Daemon::LoadDesktopFilesFrom(QString dir_path, const QProcessEnvironment &env)
{
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	QVector<QString> names;
	if (!io::ListFileNames(dir_path, names))
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
			LoadDesktopFilesFrom(full_path, env);
			continue;
		}
		
		auto *p = DesktopFile::FromPath(full_path, possible_categories_, env);
		if (p != nullptr)
		{
			auto guard = desktop_files_.guard();
			desktop_files_.hash.insert(p->GetId(), p);
		}
	}
}

void Daemon::QuitGuiApp()
{
	life_->Lock();
	life_->exit = true;
	life_->Unlock();
	QApplication::exit();// triggers qapp.exec() to return
}

void Daemon::SendAllDesktopFiles(const int fd)
{
	ByteArray reply;
	reply.add_i16(DesktopFileABI);
	{
		auto g = desktop_files_.guard();
		auto it = desktop_files_.hash.constBegin();
		while (it != desktop_files_.hash.constEnd())
		{
			DesktopFile *p = it.value();
			//mtl_printq(p->GetName());
			p->WriteTo(reply);
			it++;
		}
	}
	reply.Send(fd);
}

void Daemon::SendDesktopFilesById(ByteArray *ba, cint fd)
{
	ByteArray reply;
	reply.add_i16(DesktopFileABI);
	{
		auto g = desktop_files_.guard();
		while (ba->has_more())
		{
			QString id = ba->next_string();
			DesktopFile *p = desktop_files_.hash.value(id, nullptr);
			if (p == nullptr)
			{
				mtl_printq2("Not found by ID: ", id);
				continue;
			}
			
			reply.add_string(id);
			p->WriteTo(reply);
		}
	}
	
	reply.Send(fd);
}

void Daemon::SendDefaultDesktopFileForFullPath(ByteArray *ba, cint fd)
{
	QString full_path = ba->next_string();
	QMimeType mt = mime_db_.mimeTypeForFile(full_path);
	const QString mime = mt.name();
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	GetDesktopFilesForMime(mime, show_vec, hide_vec);
	
	ByteArray reply;
	reply.add_i16(DesktopFileABI);
	if (!show_vec.isEmpty())
	{
		DesktopFile *p = show_vec[0];
		p->WriteTo(reply);
	}
	
	reply.Send(fd);
}

void Daemon::SendOpenWithList(QString mime, cint fd)
{
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	GetDesktopFilesForMime(mime, show_vec, hide_vec);
	
	ByteArray reply;
	reply.add_i16(DesktopFileABI);
	for (DesktopFile *next: show_vec)
	{
		reply.add_i8((i8)Present::Yes);
		next->WriteTo(reply);
	}
	
	for (DesktopFile *next: hide_vec) {
		reply.add_i8((i8)Present::No);
		next->WriteTo(reply);
	}
	
	reply.Send(fd);
}

void Daemon::SysTrayClicked()
{
//	static bool flag = true;
//	tasks_win_->setVisible(flag);
//	if (flag) {
//		tasks_win_->raise();
//	}
//	flag = !flag;
	tasks_win_->setVisible(!tasks_win_->isVisible());
}

void Daemon::SetTrayVisible(const bool yes)
{
	tray_icon_->setVisible(yes);
}

} // namespace



