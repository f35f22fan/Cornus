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
media::ShortData* DecodeShort(Media *media, ByteArray &ba);
}

class Media: public QObject {
	Q_OBJECT
public:
	enum class Fill: i8 {
		AddNoneOption,
		Default
	};

	void ApplyTo(QComboBox *cb, ByteArray &ba, const media::Field f);
	void AddNTS(const QVector<QString> &names, const media::Field field);
	void Clear();
	media::Data& data() { return data_; }
	void FillInNTS(QComboBox *cb, const media::Field category,
		const Fill option = Fill::Default);
	
	QVector<QString> GetNTS(const media::Field f, const i64 ID);
	bool loaded() const { return !data_.rips.isEmpty(); }
	i64 SetNTS(const media::Field f, const i64 ID,
		const QVector<QString> &names, i64 *existing_id = nullptr,
		const media::Action action = media::Action::Insert,
		const media::Check check = media::Check::Exists);
	MutexGuard guard() { return MutexGuard(&mutex); }
	
	bool Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	void Reload();
	bool TryLock() {
		return pthread_mutex_trylock(&mutex) == 0;
	}
	void Unlock() {
		int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
	
	i32 GetMagicNumber();
	i32 magic_number_NTS() const { return data_.magic_number; }
	void Print();
	void ReloadDatabaseNTS(ByteArray &ba, media::Data &data);
	void Save();
	void WriteTo(ByteArray &ba, QComboBox *cb, const media::Field f);
	
	media::Data data_ = {};
	bool changed_by_myself_ = false;
	
Q_SIGNALS:
	void Changed();
	
private:
	void NewMagicNumber();
	void WriteAny(ByteArray &ba, const QVector<QString> &names);
	
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

}
