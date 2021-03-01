#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "../MutexGuard.hpp"
#include "socket.hh"
#include "../ElapsedTimer.hpp"

#include <pthread.h>
#include <QElapsedTimer>

namespace cornus::io {

enum class Question : i8 {
	None = 0,
	FileExists,
	WriteFailed,
};

struct TaskQuestion {
	QString file_path_in_question;
	QString explanation;
	FileExistsAnswer file_exists_answer = io::FileExistsAnswer::None;
	WriteFailedAnswer write_failed_answer = io::WriteFailedAnswer::None;
	Question question = Question::None;
};

struct TaskData {
	TaskState state = TaskState::Pause;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	ElapsedTimer work_time_recorder_ = {};
	TaskQuestion task_question_ = {};
	MutexGuard guard() { return MutexGuard(&mutex); }
	
	inline TaskState GetState(TaskQuestion *question, i64 *ret_time_worked = nullptr) {
		MutexGuard guard(&mutex);
		
		if (state == TaskState::AwatingAnswer && question != nullptr)
			*question = task_question_;
		
		if (ret_time_worked != nullptr)
			*ret_time_worked = work_time_recorder_.elapsed_ms();
		
		return state;
	}
	
	inline i64 GetTimeWorked() {
		Lock();
		i64 ret = work_time_recorder_.elapsed_ms();
		Unlock();
		return ret;
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
	
	bool ChangeState(const TaskState new_state,
		TaskQuestion *task_question = nullptr);
	
	bool WaitFor(const TaskState new_state);
};

struct Progress {
	i64 at = 0;
	i64 total = 0;
	i64 time_worked = 0;
	QString details;
	i32 details_id = -1;
	
	inline void CopyFrom(const Progress &rhs)
	{
		at = rhs.at;
		total = rhs.total;
		time_worked = rhs.time_worked;
		if (details_id != rhs.details) {
			details_id = rhs.details_id;
			details = rhs.details;
		}
	}
};

struct TaskProgress {
	Progress data = {};
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	
	inline void Get(Progress &p) {
		MutexGuard guard(&mutex);
		p.CopyFrom(data);
	}
	
	inline void AddProgress(const i64 progress, const i64 time_worked,
		i64 *total = nullptr)
	{
		MutexGuard guard(&mutex);
		data.at += progress;
		data.time_worked = time_worked;
		if (total != nullptr)
			data.total = *total;
	}
	
	inline void SetDetails(const QString &in_details) {
		MutexGuard guard(&mutex);
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
	TaskProgress& progress() { return progress_; }
	QVector<QString>& file_paths() { return file_paths_; }
	
	void ops(socket::MsgType n) { ops_ = n; }
	socket::MsgType ops() const { return ops_; }
	void StartIO();
	
private:
	NO_ASSIGN_COPY_MOVE(Task);
	Task();
	void CopyFiles();
	void CopyFileToDir(const QString &file_path, const QString &dir_path);
	void CopyRegularFile(const QString &from_path, const QString &new_dir_path,
		const QString &filename, const mode_t mode, const i64 file_size);
	void CopyXAttr(const int input_fd, const int output_fd);
	i64 CountTotalSize();
	ActUponAnswer DealWithFileExistsAnswer(const i64 file_size);
	ActUponAnswer DealWithWriteFailedAnswer(const i64 file_size);
	void DeleteFile(const QString &full_path, struct statx &stx);
	void DeleteFiles();
	bool TryAtomicMove();
	int TryCreateRegularFile(const QString &new_dir_path,
		const QString &filename, const int WriteFlags,
		const mode_t mode, const i64 file_size, QString &dest_path);
	
	TaskData data_ = {};
	TaskProgress progress_ = {};
	socket::MsgType ops_ = 0;
	QString to_dir_path_;
	QVector<QString> file_paths_;
	struct statx stx_;
};

}
