#pragma once

#include "ByteArray.hpp"
#include "err.hpp"
#include "decl.hxx"
#include "io/decl.hxx"

#include <QByteArray>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QString>

#include <pthread.h>
#include <zstd.h>
#include <webp/decode.h>

void CornusFreeQImageMemory(void *data);

namespace cornus {

namespace thumbnail {
using AbiType = i16;
const AbiType AbiVersion = 1;
// header size: abi=2 + width=2 + height=2 + bpl=2 + img_w=4 + img_h=4 + img_format=4
const i64 HeaderSize = 20;
} // thumbnail::

enum class Origin: i8 {
	TempDir,
	ExtAttr,
	DiskFile,
	Undefined
};

const bool DebugThumbnailExit = false;

struct Thumbnail {
	Thumbnail* Clone();
	QImage img;
	u64 file_id = 0;
	i64 time_generated = -1;
	TabId tab_id = -1;
	DirId dir_id = -1;
	i32 w = -1;
	i32 h = -1;
	i32 original_image_w = -1;
	i32 original_image_h = -1;
	i16 abi_version = 0;
	Origin origin = Origin::Undefined;
};

class ThumbLoaderArgs {
public:
	App *app = nullptr;
	QString full_path;
	QByteArray ext;
	ByteArray ba;
	TabId tab_id = -1;
	DirId dir_id = -1;
	io::DiskFileId file_id = {};
	int icon_w = -1;
	int icon_h = -1;
	
//	static ThumbLoaderArgs* FromFile(gui::Tab *tab,
//		io::File *file, const DirId dir_id, cint max_img_w, cint max_img_h);
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
		cint status = pthread_mutex_lock(&mutex);
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
		cint status = pthread_mutex_unlock(&mutex);
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
		cint status = pthread_cond_wait(&new_work_cond, &mutex);
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
		cint status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	bool TryLock() const {
		return (pthread_mutex_trylock(&mutex) == 0);
	}
	
	void Unlock() const {
		cint status = pthread_mutex_unlock(&mutex);
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
		cint status = pthread_cond_wait(&new_work_cond, &mutex);
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

namespace thumbnail {

// returns true on success
bool GetOriginalImageSize(ByteArray &ba, i32 &w, i32 &h);

QSize GetScaledSize(const QSize &input, cint max_img_w, cint max_img_h);

void* LoadMonitor(void *args);

QImage ImageFromByteArray(ByteArray &ba, i32 &img_w, i32 &img_h,
	AbiType &abi_version, ZSTD_DCtx *decompress_context);

Thumbnail* Load(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, cint max_img_w, cint max_img_h,
	const TabId tab_id, const DirId dir_id);

QImage LoadWebpImage(const QString &full_path, cint max_img_w,
	cint max_img_h, QSize &scaled_sz, QSize &orig_img_sz);

inline QString SizeToString(const i32 w, const i32 h, const gui::ViewMode vm)
{
	if (vm == gui::ViewMode::Details) {
		return QChar('(') + QString::number(w) + QChar('x')
			+ QString::number(h) + QChar(')');
	}
	
	return QString::number(w) + QLatin1String(" x ") + QString::number(h);
}

}} // cornus::thumbnail::
Q_DECLARE_METATYPE(cornus::Thumbnail*);
Q_DECLARE_METATYPE(cornus::ThumbLoaderArgs*);
