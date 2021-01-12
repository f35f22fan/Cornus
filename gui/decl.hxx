#pragma once

#include "../types.hxx"

#include <QMap>
#include <pthread.h>

namespace cornus::gui {
class Location;
class Table;
class TableDelegate;
class TableModel;
class ToolBar;


enum class Column : i8 {
	Icon = 0,
	FileName,
	Size,
	Count
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
