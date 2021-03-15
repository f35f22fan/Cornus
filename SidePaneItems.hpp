#pragma once

#include "decl.hxx"
#include "gui/decl.hxx"
#include "MutexGuard.hpp"

#include <QVector>
#include <pthread.h>

namespace cornus {
class SidePaneItems {
public:
	SidePaneItems();
	virtual ~SidePaneItems();
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	bool widgets_created = false;
	bool partitions_loaded = false;
	bool sidepane_model_destroyed = false;
	QVector<gui::SidePaneItem*> vec;
	
	MutexGuard guard() { return MutexGuard(&mutex); }
	
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
} // namespace
