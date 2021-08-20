#pragma once

#include "decl.hxx"
#include "gui/decl.hxx"
#include "MutexGuard.hpp"

#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QVector>
#include <pthread.h>

namespace cornus {
class TreeData {
public:
	TreeData();
	virtual ~TreeData();
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	bool widgets_created = false;
	bool partitions_loaded = false;
	bool sidepane_model_destroyed = false;
	bool bookmarks_changed_by_me = false;
	QVector<gui::TreeItem*> roots;
	
	void BroadcastPartitionsLoadedLTH();
	gui::TreeItem* GetBookmarksRootNTS(int *index = nullptr) const;
	gui::TreeItem* GetCurrentPartitionNTS() const;
	gui::TreeItem* GetPartitionByMountPathNTS(const QString &full_path);
	MutexGuard guard() { return MutexGuard(&mutex, LockType::Normal); }
	MutexGuard try_guard();
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_warn("pthreads_mutex_lock: %s", strerror(status));
		return status;
	}
	
	void MarkCurrentPartition(const QString &full_path);
	
	inline int Unlock() {
		return pthread_mutex_unlock(&mutex);
	}
};
} // namespace


