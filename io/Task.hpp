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

QString ToString(const io::TaskState state);

struct Answer {
	Answer() {bits_ = 0;}
	Answer& operator=(const Answer &rhs) {
		this->bits_ = rhs.bits_;
		return *this;
	}
	u8 bits_ = 0;
	
	u8 bits() const { return bits_; }
	void add(const Answer &rhs) { bits_ |= rhs.bits_; }
	void clear() { bits_ = 0; }
	bool none() const { return bits_ == 0; }
	
	cu8 OverwriteBit = 1 << 0;
	bool overwrite() const { return bits_ & OverwriteBit; }
	void overwrite(cbool flag) { if (flag) { bits_ |= OverwriteBit;} else {bits_ &= ~OverwriteBit;} }
	
	cu8 OverwriteAllBit = 1 << 1;
	bool overwrite_all() const { return bits_ & OverwriteAllBit; }
	void overwrite_all(cbool flag) { if (flag) { bits_ |= OverwriteAllBit;} else {bits_ &= ~OverwriteAllBit;} }
	
	cu8 SkipBit = 1 << 2;
	bool skip() const { return bits_ & SkipBit; }
	void skip(cbool flag) { if (flag) { bits_ |= SkipBit;} else {bits_ &= ~SkipBit;} }
	
	cu8 SkipAllBit = 1 << 3;
	bool skip_all() const { return bits_ & SkipAllBit; }
	void skip_all(cbool flag) { if (flag) { bits_ |= SkipAllBit;} else {bits_ &= ~SkipAllBit;} }
	
	bool any_skip() const { return bits_ & (SkipAllBit | SkipBit); }
	
	cu8 AutoRenameBit = 1 << 4;
	bool auto_rename() const { return bits_ & AutoRenameBit; }
	void auto_rename(cbool flag) { if (flag) { bits_ |= AutoRenameBit;} else {bits_ &= ~AutoRenameBit;} }
	
	cu8 AutoRenameAllBit = 1 << 5;
	bool auto_rename_all() const { return bits_ & AutoRenameAllBit; }
	void auto_rename_all(cbool flag) { if (flag) { bits_ |= AutoRenameAllBit;} else {bits_ &= ~AutoRenameAllBit;} }
	
	cu8 AbortBit = 1 << 6;
	bool abort() const { return bits_ & AbortBit; }
	void abort(cbool flag) { if (flag) { bits_ |= AbortBit;} else {bits_ &= ~AbortBit;} }
	
	cu8 RetryBit = 1 << 7;
	bool retry() const { return bits_ & RetryBit; }
	void retry(cbool flag) { if (flag) { bits_ |= RetryBit;} else {bits_ &= ~RetryBit;} }
	
	bool retry_or_overwrite() const { return bits_ & (RetryBit | OverwriteBit | OverwriteAllBit); }
};

enum class Question : i8 {
	None = 0,
	FileExists,
	WriteFailed,
	DeleteFailed,
	AccessPermission,
};

struct TaskQuestion {
	QString file_path_in_question;
	QString explanation;
	Question question = Question::None;
};

struct TaskData {
	TaskState state = TaskState::Pause;
	CondMutex cm = {};
	ElapsedTimer work_time_recorder_ = {};
	TaskQuestion task_question_ = {};
	Answer answer_ = {};
	
	Answer GetAnswerWithLock() {
		auto g = cm.guard();
		return answer_;
	}
	
	inline TaskState GetState(TaskQuestion *question = nullptr, i64 *ret_time_worked = nullptr)
	{
		auto g = cm.guard();
		if (question != nullptr && state == TaskState::AwaitingAnswer)
			*question = task_question_;
		
		if (ret_time_worked != nullptr)
			*ret_time_worked = work_time_recorder_.elapsed_ms();
		
		return state;
	}
	
	inline i64 GetTimeWorked() {
		auto g = cm.guard();
		return work_time_recorder_.elapsed_ms();
	}
	
	void ChangeState(const TaskState new_state, Answer *answer = nullptr, TaskQuestion *question = nullptr);
	void RemoveBits(const TaskState states);
	TaskState WaitFor(const TaskState new_state, const Lock l = Lock::Yes);
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
		if (details_id != rhs.details_id) {
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
	
	void GetShort(i64 &at, i64 &total) {
		MutexGuard guard(&mutex);
		at = data.at;
		total = data.total;
	}
	
	inline void AddProgress(const i64 progress, const i64 time_worked,
		const i64 *new_total = nullptr)
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
	enum class InitTotalSize: i8 {
		Yes,
		No
	};
	
public:
	~Task();
	static Task* From(cornus::ByteArray &ba, const HasSecret hs);
	
	TaskData& data() { return data_; }
	void GetShort(i64 &at, i64 &total) { progress_.GetShort(at, total); }
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
		const QString &filename, const mode_t mode, const i64 file_size);
	void CopyXAttr(const int input_fd, const int output_fd);
	i64 CountTotalSize();
	
	Answer WaitForDeleteFailedAnswer(const i64 file_size);
	Answer WaitForFileAccessAnswer(ci64 file_size, QString dest_path);
	Answer WaitForFileExistsAnswer(QString dir_path, QString filename, QString dest_path,
		int &file_flags, ci64 file_size, mode_t mode);
	Answer WaitForWriteFailedAnswer(const i64 file_size);
	
	// returns 0 on success, errno otherwise
	int DeleteFile(const QString &full_path, struct statx &stx, QString &problematic_file);
	void DeleteFiles(const InitTotalSize its);
	bool FixDestDir();
	void MoveToTrash();
	bool TryAtomicMove();
	int TryCreateRegularFile(const QString &new_dir_path,
		const QString &filename, const int WriteFlags,
		const mode_t mode, const i64 file_size, QString &dest_path);
	
	TaskData data_ = {};
	TaskProgress progress_ = {};
	io::MessageType ops_ = 0;
	QString to_dir_path_;
	QVector<QString> file_paths_;
	QList<QUrl> urls_;
	struct statx stx_;
};

}
