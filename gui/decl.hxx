#pragma once

#include "../err.hpp"

#include <QMap>
#include <pthread.h>

namespace cornus::gui {
class Hiliter;
class Location;
class SidePane;
class SidePaneItem;
class SidePaneModel;
class Table;
class TableDelegate;
class TableModel;
class TaskGui;
class TasksWin;
class TextEdit;
class ToolBar;

enum class HiliteMode: i16 {
	None = -1,
/// First viable mode must start at zero because these are
/// also used as indices into a vector:
	C_CPP = 0,
	PlainText,
	Python,
	ShellScript,
	Count
};

struct SidePaneItems {
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	bool widgets_created = false;
	QVector<SidePaneItem*> vec;
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_warn("pthreads_mutex_lock: %s", strerror(status));
		return status;
	}
	
	inline int Unlock() {
		return pthread_mutex_unlock(&mutex);
	}
};

enum class Column : i8 {
	Icon = 0,
	FileName,
	Size,
	TimeCreated,
	TimeModified,
	Count,
};

struct Notify {
	int fd = -1;
/** Why a map? => inotify_add_watch() only adds a new watch if
there's no previous watch watching the same location, otherwise it
returns an existing descriptor which is likely used by some other code.
Therefore the following bug can happen:
if two code paths register/watch the same location thru the same
inotify instance at the same time and one of them removes the
watch - the other code path will also lose ability to watch that
same location. Quite a subtle bug. Therefore use a map that acts like
a refcounter: when a given watch FD goes to zero it's OK to remove it,
otherwise just decrease it by 1.*/
	QMap<int, int> watches;
	pthread_mutex_t watches_mutex = PTHREAD_MUTEX_INITIALIZER;
};
}
