#pragma once

#include "../err.hpp"
#include "../MutexGuard.hpp"

#include <QMap>
#include <QVector>
#include <pthread.h>
#include <sys/inotify.h>

namespace cornus::io {

class Notify {
public:
	QVector<const char*> MaskToString(const u4 mask);
	void Close();
	MutexGuard guard() { return MutexGuard(&mutex); }
	void Init();
	
	int fd = -1;
/** Why a map? => inotify_add_watch() only adds a new watch if
there's no previous watch watching the same location, otherwise it
returns an existing descriptor which is likely used by some other code.
Therefore the following bug can happen:
if two code paths register/watch the same location thru the same
inotify instance at the same time and one of them removes the
watch - the other code path will also lose ability to watch that
same location. That's quite a subtle bug. Therefore use a map that
acts like a refcounter: when a given watch FD goes to zero it's OK
to remove it, otherwise just decrease it by 1.*/
	QMap<int, int> watches;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

class AutoRemoveWatch {
public:
	AutoRemoveWatch(io::Notify &notify, int wd);
	virtual ~AutoRemoveWatch();
	
	void RemoveWatch(int wd) {
		notify_.watches.remove(wd); // needed on IN_UNMOUNT event
	}
	
private:
	io::Notify &notify_;
	int wd_ = -1;
};

} // [cornus::io::]
