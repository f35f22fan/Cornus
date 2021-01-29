#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "../MutexGuard.hpp"
#include "socket.hh"

#include <pthread.h>

namespace cornus::io {

enum class TaskState: u8 {
	Continue,
	Pause,
	Stop,
	Error,
	Finished,
	QueryFailed /// query of this state failed in the parent class
};

struct TaskData {
	TaskState state = TaskState::Pause;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	
	inline TaskState GetState() {
		MutexGuard guard(&mutex);
		return state;
	}
	
	inline bool Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	inline bool Unlock() {
		int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	bool ContinueNTS() const { return state == TaskState::Continue; }
	bool FinishedNTS() const { return state == TaskState::Finished; }
	bool ChangeState(const TaskState new_state);
	bool WaitFor(const TaskState new_state);
};

struct Progress {
	i64 at = 0;
	i64 total = 0;
	QString details;
	i32 details_id = -1;
	time_t time_created = 0;
	
	inline void CopyFrom(const Progress &rhs)
	{
		at = rhs.at;
		total = rhs.total;
		if (details_id != rhs.details) {
			details_id = rhs.details_id;
			details = rhs.details;
			time_created = rhs.time_created;
		}
	}
};

struct TaskProgress {
	Progress data = {};
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	
	inline void GetProgress(Progress &p) {
		MutexGuard guard(&mutex);
		p.CopyFrom(data);
	}
	
	inline void AddProgress(const i64 progress, i64 *total = nullptr) {
		MutexGuard guard(&mutex);
		data.at += progress;
		if (total != nullptr)
			data.total = *total;
	}
	
	inline void SetDetails(const QString &in_details) {
		data.details = in_details;
		data.details_id++;
	}
	
	inline bool Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	inline bool Unlock() {
		int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
};

class Task {
public:
	~Task();
	static Task* From(cornus::ByteArray &ba);
	
	TaskData& data() { return data_; }
	
	QVector<QString>& file_paths() { return file_paths_; }
	
	void ops(socket::MsgType n) { ops_ = n; }
	socket::MsgType ops() const { return ops_; }
	void StartIO();
///	void WaitForStartSignal();
	
private:
	NO_ASSIGN_COPY_MOVE(Task);
	Task();
	void CopyFiles();
	void CopyFileToDir(const QString &file_path, const QString &dir_path);
	void CopyRegularFile(const QString &from_path, const QString &dest_path,
		const mode_t mode, const i64 file_size);
	i64 CountTotalSize();
	bool TryAtomicMove();
	
	TaskData data_ = {};
	TaskProgress progress_ = {};
	socket::MsgType ops_ = 0;
	QString to_dir_path_;
	QVector<QString> file_paths_;
	struct statx stx_;
};

}
