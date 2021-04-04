#pragma once

#include "decl.hxx"
#include "media.hxx"
#include "MutexGuard.hpp"

#include <QComboBox>
#include <QMap>
#include <QVector>
#include <pthread.h>

namespace cornus {

namespace media {
void Reload(App *app);
}

/** It must be a QMap (not QHash) because
 "When iterating over a QMap, the items are always sorted by key. With QHash,
 the items are arbitrarily ordered."
 Thus when inserting a new item with its ID as the current QMap size it works.
 Hence only a QMap can have the (implied) ID not change.
 Note: QMap items may never be removed, only their names changed.
*/
using HashU8 = QMap<u8, QString>;
using HashU16 = QMap<u16, QVector<QString>>;
using HashU32 = QMap<u32, QVector<QString>>;

class Media {
public:
	void ApplyTo(QComboBox *cb, ByteArray &ba, const media::Field f);
	void AddNTS(const QVector<QString> &names, const media::Field field);
	void Clear();
	void FillInNTS(QComboBox *cb, const media::Field category);
	
	QVector<QString> GetNTS(const media::Field f, const i64 ID);
	i64 SetNTS(const media::Field f, const i64 ID,
		const QVector<QString> &names, const media::Action action = media::Action::Insert);
	MutexGuard guard() { return MutexGuard(&mutex); }
	
	bool Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	bool TryLock() {
		return pthread_mutex_trylock(&mutex) == 0;
	}
	
	void Unlock() {
		int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
	
	i32 GetMagicNumber();
	i32 magic_number_NTS() const { return magic_number_; }
	void Print();
	void ReloadDatabaseNTS(ByteArray &ba);
	void Save();
	void WriteTo(ByteArray &ba, QComboBox *cb, const media::Field f);
	
	HashU16 genres_;
	HashU16 subgenres_;
	HashU16 countries_;
	
	HashU32 actors_;
	HashU32 directors_;
	HashU32 writers_;
	
	HashU8 rips_;
	HashU8 video_codecs_;
	
	bool loaded_ = false;
	bool changed_by_myself_ = false;
	
private:
	void NewMagicNumber();
	void WriteAny(ByteArray &ba, const QVector<QString> &names);
	
	i32 magic_number_ = -1;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

}
