#pragma once

#include "err.hpp"
#include "decl.hxx"
#include "io/decl.hxx"

#include <QByteArray>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QString>

#include <pthread.h>

namespace cornus {

enum class FromTempDir: i8 {
	Yes,
	No,
	Undefined
};

// header size: width=4 + height=4 + bpl=4 + img_format=4
const int ThumbnailHeaderSize = 16;
const bool DebugThumbnailExit = false;

struct Thumbnail {
	QImage img;
	u64 file_id = 0;
	i64 time_generated = -1;
	TabId tab_id = -1;
	DirId dir_id = -1;
	int w = -1;
	int h = -1;
	FromTempDir from_temp = FromTempDir::Undefined;
};

struct ThumbLoaderArgs {
	App *app = nullptr;
	QString full_path;
	QByteArray ext;
	TabId tab_id = -1;
	DirId dir_id = -1;
	io::DiskFileId file_id = {};
	int icon_w = -1;
	int icon_h = -1;
};

struct GlobalThumbLoaderData;

struct ThumbLoaderData {
	GlobalThumbLoaderData *global_data = nullptr;
	ThumbLoaderArgs *new_work = nullptr;
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	mutable pthread_cond_t new_work_cond = PTHREAD_COND_INITIALIZER;
	bool wait_for_work = true;
	bool thread_exited = false;
	
	bool Lock() const {
		const int status = pthread_mutex_lock(&mutex);
		if (status != 0) {
			printf("ThumbLoaderData::Lock() failed!\n");
			mtl_status(status);
		}
		return status == 0;
	}
	
	bool TryLock() const {
		return (pthread_mutex_trylock(&mutex) == 0);
	}
	
	void Unlock() const {
		const int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
	
	MutexGuard guard() const {
		return MutexGuard(&mutex);
	}
	
	bool Broadcast() {
		return (pthread_cond_broadcast(&new_work_cond) == 0);
	}
	
	int CondWaitForNewWork() const {
		const int status = pthread_cond_wait(&new_work_cond, &mutex);
		if (status != 0)
			mtl_status(status);
		return status;
	}
	
	bool SignalNewWorkAvailable() {
		return (pthread_cond_signal(&new_work_cond) == 0);
	}
};

struct GlobalThumbLoaderData {
	QVector<ThumbLoaderData*> threads;
	QVector<ThumbLoaderArgs*> work_queue;
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	mutable pthread_cond_t new_work_cond = PTHREAD_COND_INITIALIZER;
	
	bool Lock() const {
		const int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	bool TryLock() const {
		return (pthread_mutex_trylock(&mutex) == 0);
	}
	
	void Unlock() const {
		const int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
	
	MutexGuard guard() const {
		return MutexGuard(&mutex);
	}
	
	bool Broadcast() {
		return (pthread_cond_broadcast(&new_work_cond) == 0);
	}
	
	int CondWaitForWorkQueueChange() const {
		const int status = pthread_cond_wait(&new_work_cond, &mutex);
		if (status != 0)
			mtl_status(status);
		return status;
	}
	
	int CondTimedWait(const struct timespec *ts) const {
		return pthread_cond_timedwait(&new_work_cond, &mutex, ts);
	}
	
	bool SignalWorkQueueChanged() const {
		return (pthread_cond_signal(&new_work_cond) == 0);
	}
};

void* GlobalThumbLoadMonitor(void *args);

Thumbnail* LoadThumbnail(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, const int icon_w, const int icon_h,
	const TabId tab_id, const DirId dir_id);

namespace thumbnail {
QImage ImageFromByteArray(ByteArray &ba);
}

}
Q_DECLARE_METATYPE(cornus::Thumbnail*);
Q_DECLARE_METATYPE(cornus::ThumbLoaderArgs*);
