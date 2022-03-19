#pragma once

#include "../CondMutex.hpp"
#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "io.hh"
#include "../MutexGuard.hpp"
#include "socket.hh"
#include "../ElapsedTimer.hpp"

#include <pthread.h>
#include <QElapsedTimer>

namespace cornus::io {

enum class Question : i1 {
	None = 0,
	FileExists,
	WriteFailed,
	DeleteFailed,
};

struct TaskQuestion {
	QString file_path_in_question;
	QString explanation;
	DeleteFailedAnswer delete_failed_answer = io::DeleteFailedAnswer::None;
	FileExistsAnswer file_exists_answer = io::FileExistsAnswer::None;
	WriteFailedAnswer write_failed_answer = io::WriteFailedAnswer::None;
	Question question = Question::None;
};

struct TaskData {
	TaskState state = TaskState::Pause;
	CondMutex cm = {};
	ElapsedTimer work_time_recorder_ = {};
	TaskQuestion task_question_ = {};
	
	inline TaskState GetState(TaskQuestion *question = nullptr, i8 *ret_time_worked = nullptr)
	{
		auto g = cm.guard();
		if (question != nullptr && state == TaskState::AwaitingAnswer)
			*question = task_question_;
		
		if (ret_time_worked != nullptr)
			*ret_time_worked = work_time_recorder_.elapsed_ms();
		
		return state;
	}
	inline i8 GetTimeWorked() {
		cm.Lock();
		i8 ret = work_time_recorder_.elapsed_ms();
		cm.Unlock();
		return ret;
	}
	void ChangeState(const TaskState new_state, TaskQuestion *task_question = nullptr);
	
	void RemoveBits(const TaskState states);
	TaskState WaitFor(const TaskState new_state, const Lock l = Lock::Yes);
};

struct Progress {
	i8 at = 0;
	i8 total = 0;
	i8 time_worked = 0;
	QString details;
	i4 details_id = -1;
	
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
	
	inline void AddProgress(const i8 progress, const i8 time_worked,
		const i8 *new_total = nullptr)
	{
		MutexGuard g(&mutex);
		data.at += progress;
		data.time_worked = time_worked;
		if (new_total != nullptr)
			data.total = *new_total;
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
	
	enum class InitTotalSize: i1 {
		Yes,
		No
	};
	
public:
	~Task();
	static Task* From(cornus::ByteArray &ba, const HasSecret hs);
	
	TaskData& data() { return data_; }
	TaskProgress& progress() { return progress_; }
	QVector<QString>& file_paths() { return file_paths_; }
	
	void ops(io::MessageType n) { ops_ = n; }
	io::MessageType ops() const { return ops_; }
	void StartIO();
	
	void SetDefaultAction(const IOAction action);
	
private:
	NO_ASSIGN_COPY_MOVE(Task);
	Task();
	void CopyFiles();
	void CopyFileToDir(const QString &file_path, const QString &dir_path);
	void CopyRegularFile(const QString &from_path, const QString &new_dir_path,
		const QString &filename, const mode_t mode, const i8 file_size);
	void CopyXAttr(const int input_fd, const int output_fd);
	i8 CountTotalSize();
	ActUponAnswer DealWithDeleteFailedAnswer(const i8 file_size);
	ActUponAnswer DealWithFileExistsAnswer(const i8 file_size);
	ActUponAnswer DealWithWriteFailedAnswer(const i8 file_size);
	
	// returns 0 on success, errno otherwise
	int DeleteFile(const QString &full_path, struct statx &stx, QString &problematic_file);
	void DeleteFiles(const InitTotalSize its);
	bool FixDestDir();
	void MoveToTrash();
	bool TryAtomicMove();
	int TryCreateRegularFile(const QString &new_dir_path,
		const QString &filename, const int WriteFlags,
		const mode_t mode, const i8 file_size, QString &dest_path);
	
	TaskData data_ = {};
	TaskProgress progress_ = {};
	io::MessageType ops_ = 0;
	QString to_dir_path_;
	QVector<QString> file_paths_;
	struct statx stx_;
};

}
