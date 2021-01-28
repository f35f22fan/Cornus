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
	
	bool Continue() const { return state == TaskState::Continue; }
	bool Finished() const { return state == TaskState::Finished; }
	bool ChangeState(const TaskState new_state);
	bool WaitFor(const TaskState new_state);
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
	void WaitForStartSignal();
	
private:
	NO_ASSIGN_COPY_MOVE(Task);
	Task();
	
	TaskData data_ = {};
	socket::MsgType ops_;
	QString to_dir_path_;
	QVector<QString> file_paths_;
	
	
};

}
