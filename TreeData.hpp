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
	
	int Broadcast() { return pthread_cond_broadcast(&cond); }
	inline int CondWait() {
		return pthread_cond_wait(&cond, &mutex);
	}
	
	gui::TreeItem* GetBookmarksRoot(int *index = nullptr) const;
	gui::TreeItem* GetCurrentPartition() const;
	gui::TreeItem* GetPartitionByMountPath(const QString &full_path);
	
	MutexGuard guard() { return MutexGuard(&mutex, LockType::Normal); }
	
	inline bool Lock() {
		const int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_warn("pthreads_mutex_lock: %s", strerror(status));
		return (status == 0);
	}
	
	void MarkCurrentPartition(const QString &full_path);
	
	inline int Unlock() {
		return pthread_mutex_unlock(&mutex);
	}
	
private:
	
	MutexGuard try_guard();
};
} // namespace


