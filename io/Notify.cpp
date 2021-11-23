#include "Notify.hpp"

namespace cornus::io {

void Notify::Init()
{
	if (fd == -1)
	{
		fd = inotify_init();
		if (fd == -1)
			mtl_status(errno);
	}
}

void Notify::Close()
{
	if (fd != -1 && (pthread_mutex_lock(&mutex) == 0)) {
		close(fd);
		fd = -1;
		pthread_mutex_unlock(&mutex);
	}
}

QVector<const char*>
Notify::MaskToString(const u32 mask)
{
	QVector<const char*> v;
	if (mask & IN_ACCESS)
		v.append("IN_ACCESS");
	if (mask & IN_OPEN)
		v.append("IN_OPEN");
	if (mask & IN_CREATE)
		v.append("IN_CREATE");
	if (mask & IN_DELETE)
		v.append("IN_DELETE");
	if (mask & IN_DELETE_SELF)
		v.append("IN_DELETE_SELF");
	if (mask & IN_MOVE_SELF)
		v.append("IN_MOVE_SELF");
	if (mask & IN_MOVED_FROM)
		v.append("IN_MOVED_FROM");
	if (mask & IN_MOVED_TO)
		v.append("IN_MOVED_TO");
	if (mask & IN_Q_OVERFLOW)
		v.append("IN_Q_OVERFLOW");
	if (mask & IN_UNMOUNT)
		v.append("IN_UNMOUNT");
	if (mask & IN_CLOSE_NOWRITE)
		v.append("IN_CLOSE_NOWRITE");
	if (mask & IN_CLOSE_WRITE)
		v.append("IN_CLOSE_WRITE");
	if (mask & IN_IGNORED)
		v.append("IN_IGNORED");
	if (mask & IN_DONT_FOLLOW)
		v.append("IN_DONT_FOLLOW");
	if (mask & IN_MASK_ADD)
		v.append("IN_MASK_ADD");
	if (mask & IN_ONESHOT)
		v.append("IN_ONESHOT");
	if (mask & IN_ONLYDIR)
		v.append("IN_ONLYDIR");
	if (mask & IN_MASK_CREATE)
		v.append("IN_MASK_CREATE");
	if (mask & IN_ISDIR)
		v.append("IN_ISDIR");
	if (mask & IN_Q_OVERFLOW)
		v.append("IN_Q_OVERFLOW");
	
	return v;
}

AutoRemoveWatch::AutoRemoveWatch(io::Notify &notify, int wd):
	notify_(notify), wd_(wd)
{
	MutexGuard guard = notify_.guard();
	const int count = notify_.watches.value(wd_, 0);
	notify_.watches[wd_] = count + 1;
}

AutoRemoveWatch::~AutoRemoveWatch()
{
	MutexGuard guard = notify_.guard();
	const bool contains_wd = notify_.watches.contains(wd_);
	const int count_subtracted = notify_.watches.value(wd_) - 1;
	
	if (count_subtracted > 0) {
		notify_.watches[wd_] = count_subtracted;
		return;
	}

	notify_.watches.remove(wd_);
	
	if (contains_wd) {
		int status = inotify_rm_watch(notify_.fd, wd_);
		if (status != 0)
			mtl_warn("%s: %d", strerror(errno), wd_);
	}
}

} // [cornus::io::]
