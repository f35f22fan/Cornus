#include "Task.hpp"

#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "File.hpp"
#include "io.hh"
#include "../trash.hh"

#include <QObject>

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE /// for copy_file_range() as of Linux 5.3+
#endif
#include <unistd.h>

namespace cornus::io {

bool TaskData::ChangeState(const TaskState new_state,
	TaskQuestion *task_question)
{
///mtl_info("TaskData::ChangeState to: %u", u16(new_state));
	MutexGuard guard(&mutex);
	if (task_question != nullptr)
		task_question_ = *task_question;
	state = new_state;
	if (new_state & (TaskState::Finished | TaskState::Pause
		| TaskState::Abort | TaskState::AwatingAnswer)) {
		work_time_recorder_.Pause();
	} else if (new_state & (TaskState::Continue | TaskState::Answered)) {
		work_time_recorder_.Continue();
	}
	
	int status = pthread_cond_broadcast(&cond);
	if (status != 0) {
		mtl_status(status);
		return false;
	}

	return true;
}

bool TaskData::WaitFor(const TaskState new_state)
{
	if (!Lock())
		return false;
	
	bool succeeded = true;
	
	while (!(state & new_state)) {
		int status = pthread_cond_wait(&cond, &mutex);
		if (status != 0) {
			mtl_status(status);
			succeeded = false;
			break;
		}
	}
	
	Unlock();
	
	return succeeded;
}

Task::Task() {}

Task::~Task() {}

void Task::CopyFiles()
{
	i64 total_size = CountTotalSize();
	if (total_size < 0) {
		return;
	}
	
	progress_.AddProgress(0, 0, &total_size);
	for (const auto &path: file_paths_) {
		CopyFileToDir(path, to_dir_path_);
	}
}

void Task::CopyFileToDir(const QString &file_path, const QString &in_dir_path)
{
	int last_slash_index = file_path.lastIndexOf('/');
	if (last_slash_index == -1) {
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	const QStringRef file_name = file_path.midRef(last_slash_index + 1);
	/// PROGRESS>>
	progress_.SetDetails(file_name.toString());
	/// PROGRESS<<
	
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto file_ba = file_path.toLocal8Bit();
	
	if (statx(0, file_ba.data(), flags, fields, &stx_) != 0) {
		mtl_warn("statx(): %s: %s", strerror(errno), file_ba.data());
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	const i64 file_size = stx_.stx_size;
	const auto mode = stx_.stx_mode;
	i64 time_worked = 0;
	
	QString new_dir_path = in_dir_path;
	if (!new_dir_path.endsWith('/'))
		new_dir_path.append('/');
	
	if (S_ISDIR(mode)) {
		
		QVector<QString> names;
		if (io::ListFileNames(file_path, names) != io::Err::Ok) {
			data_.ChangeState(TaskState::Error);
			return;
		}
		
		new_dir_path.append(file_name);
		auto dir_ba = new_dir_path.toLocal8Bit();
		
		int status = mkdir(dir_ba.data(), mode);
		if (status != 0) {
			if (errno != EEXIST) {
				mtl_status(errno);
				data_.ChangeState(TaskState::Error);
				return;
			}
		}
		auto state = data_.GetState(nullptr, &time_worked);
		Q_UNUSED(state);
		progress_.AddProgress(file_size, time_worked);
		
		for (const auto &name: names)
		{
			CopyFileToDir(file_path + '/' + name, new_dir_path);
			if (data_.GetState(nullptr) == TaskState::Error)
				return;
		}
	} else if (S_ISREG(mode)) {
		CopyRegularFile(file_path, new_dir_path, file_name.toString(), mode, file_size);
	} else if (S_ISLNK(mode)) {
		
		QString link_target_path;
		if (ReadLinkSimple(file_ba.data(), link_target_path)) {
			auto target_path_ba = link_target_path.toLocal8Bit();
			auto new_file_path = (new_dir_path + file_name).toLocal8Bit();
			int status = symlink(target_path_ba.data(), new_file_path.data());
			
			if (status != 0)
			{
				mtl_status(errno);
				data_.ChangeState(TaskState::Error);
				return;
			}
			auto state = data_.GetState(nullptr, &time_worked);
			Q_UNUSED(state);
			progress_.AddProgress(file_size, time_worked);
		}
	}
}

void Task::CopyRegularFile(const QString &from_path, const QString &new_dir_path,
	const QString &filename, const mode_t mode, const i64 file_size)
{
	auto from_ba = from_path.toLocal8Bit();
	int input_fd = ::open(from_ba.data(), O_RDONLY | O_LARGEFILE);
	
	if (input_fd == -1) {
		mtl_warn("%s: %s", from_ba.data(), strerror(errno));
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	AutoCloseFd input_ac(input_fd);
	const int WarnIfExistsFlags = O_CREAT | O_EXCL | O_LARGEFILE
		| O_NOFOLLOW | O_NOATIME | O_WRONLY;
	QString dest_path;
	int output_fd = TryCreateRegularFile(new_dir_path, filename,
		WarnIfExistsFlags, mode, file_size, dest_path);
	
	if (output_fd == -1)
		return;
	
	AutoCloseFd output_ac(output_fd);
	i64 so_far = 0;
	loff_t in_off = 0, out_off = 0;
	const usize chunk = 512 * 128;
	
#ifdef CORNUS_THROTTLE_IO
	struct timespec throttle_ts;
	throttle_ts.tv_sec = 0;
	throttle_ts.tv_nsec = 300l * 1000000l;
#endif
	i64 time_worked;
	
	while (so_far < file_size)
	{
		isize count = copy_file_range(input_fd, &in_off, output_fd, &out_off, chunk, 0);
		if (count == -1) {
			if (errno == EAGAIN)
				continue;
			
			data_.Lock();
			const auto write_failed_answer = data_.task_question_.write_failed_answer;
			data_.Unlock();
			
			if (write_failed_answer == WriteFailedAnswer::SkipAll) {
				mtl_info("Skip All");
				return;
			}
			
			TaskQuestion question = {};
			question.explanation = strerror(errno);
			question.file_path_in_question = dest_path;
			question.question = io::Question::WriteFailed;
			data_.ChangeState(TaskState::AwatingAnswer, &question);
			ActUponAnswer answer = DealWithWriteFailedAnswer(file_size);
			if (answer == ActUponAnswer::Retry) {
				continue;
			} else if (answer == ActUponAnswer::Skip) {
				return;
			} else if (answer == ActUponAnswer::Abort) {
				return;
			}
			return;
		} else if (count == 0) {
			break;
		}
		
		so_far += count;
		auto state = data_.GetState(nullptr, &time_worked);
		progress_.AddProgress(count, time_worked);
		
		if (state & TaskState::Pause) {
			mtl_info("Waiting for Continue state");
			if (!data_.WaitFor(TaskState::Continue | TaskState::Abort)) {
				mtl_trace();
			}
			mtl_info("Done waiting");
		} else if (state & TaskState::Abort) {
			mtl_info("Got cancel state, returning..");
			return;
		}
		
#ifdef CORNUS_THROTTLE_IO
		int status = clock_nanosleep(CLOCK_REALTIME, 0, &throttle_ts, NULL);
		if (status != 0)
			mtl_status(status);
#endif
	}
	
	CopyXAttr(input_fd, output_fd);
}

void
Task::CopyXAttr(const int input_fd, const int output_fd)
{
	isize buflen = flistxattr(input_fd, NULL, 0);
	if (buflen == 0)
		return; /// no attributes
	
	if (buflen == -1)
	{
		mtl_status(errno);
		return;
	}
	
	/// Allocate the buffer.
	char *buf = (char*)malloc(buflen);
	CHECK_PTR_VOID(buf);
	
	AutoFree af(buf);
	/// Copy the list of attribute keys to the buffer.
	buflen = flistxattr(input_fd, buf, buflen);
	CHECK_TRUE_VOID((buflen != -1));
	
	/** Loop over the list of zero terminated strings with the
		attribute keys. Use the remaining buffer length to determine
		the end of the list. */
	char *key = buf;
	while (buflen > 0)
	{
		/// output attribute key.
		///mtl_info("key: \"%s\"", key);
		
		/// Determine length of the value.
		isize vallen = fgetxattr(input_fd, key, NULL, 0);
		if (vallen <= 0)
			break;
		
		/// One extra byte is needed to append 0x00.
		char *val = (char*) malloc(vallen + 1);
		if (val == NULL)
		{
			mtl_status(errno);
			break;
		}
		AutoFree af_val(val);
		
		/// Copy value to buffer.
		vallen = fgetxattr(input_fd, key, val, vallen);
		if (vallen == -1)
		{
			mtl_status(errno);
			break;
		}
		
		val[vallen] = 0;
		int status = fsetxattr(output_fd, key, val, vallen, 0);
		if (status != 0)
		{
			 /// usually fails on "security.capabilities"
			if (errno != EPERM)
				mtl_status(errno);
		}
		
		/// Forward to next attribute key.
		const isize keylen = strlen(key) + 1;
		buflen -= keylen;
		key += keylen;
	}
}

i64 Task::CountTotalSize()
{
	io::CountRecursiveInfo info = {};
	for (const auto &path: file_paths_)
	{
		if (!io::CountSizeRecursive(path, stx_, info)) {
			mtl_printq(path);
			return -1;
		}
	}
	
	return info.size;
}

void Task::DeleteFile(const QString &full_path, struct statx &stx)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto ba = full_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("%s", strerror(errno));
		return;
	}
	
	if (S_ISDIR(stx.stx_mode)) {
		QString dir_path = full_path;
		if (!dir_path.endsWith('/'))
			dir_path.append('/');
		
		QVector<QString> names;
		io::ListFileNames(dir_path, names);
		
		for (const auto &name: names) {
			auto fp = dir_path + name;
			DeleteFile(fp, stx);
		}
	}
	
	int status = remove(ba.data());
	if (status != 0)
		mtl_warn("%s", strerror(errno));
}

void Task::DeleteFiles() {
	for (const auto &path: file_paths_)
		DeleteFile(path, stx_);
}

Task* Task::From(cornus::ByteArray &ba)
{
	auto *task = new Task();
	task->ops_ = ba.next_u32();
	
	if (task->ops_ != (io::MessageType)io::Message::DeleteFiles &&
		task->ops_ != (io::MessageType)io::Message::MoveToTrash)
	{
		task->to_dir_path_ = ba.next_string();
	}
	
	while(ba.has_more()) {
		task->file_paths_.append(ba.next_string());
	}
	
	return task;
}

void Task::MoveToTrash()
{
	if (file_paths_.isEmpty())
		return;
	
	QString trash_path = trash::EnsureTrashForFile(file_paths_[0]);
	if (trash_path.isEmpty())
		return;
	if (!trash_path.endsWith('/'))
		trash_path.append('/');
	
	const i64 now = time(NULL);
	const QString time_str = trash::time_to_str(now) + '_';
	
	for (auto &next: file_paths_)
	{
		QStringRef name = io::GetFileNameOfFullPath(next);
		auto new_path = (trash_path + time_str + name).toLocal8Bit();
		auto old_path = next.toLocal8Bit();
		//mtl_info("new_path: %s\n, old_path: %s", new_path.data(), old_path.data());
		int status = rename(old_path.data(), new_path.data());
		if (status != 0) {
			mtl_status(status);
			continue;
		}
	}
}

void Task::StartIO()
{
	data_.ChangeState(TaskState::Continue);
	if (to_dir_path_.startsWith('/') && !to_dir_path_.endsWith('/'))
		to_dir_path_.append('/');
	
	if (file_paths_.isEmpty()) {
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	
	const QString &first = file_paths_[0];
	auto name = io::GetFileNameOfFullPath(first);
	QString parent = first.mid(0, first.size() - name.size());
	if (name.isEmpty() || io::SameFiles(parent, to_dir_path_)) {
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	if (ops_ == (MessageType)Message::DeleteFiles) {
		DeleteFiles();
	} else if (ops_ == (MessageType)Message::MoveToTrash) {
		MoveToTrash();
	} else if (ops_ & (MessageType)Message::Copy) {
#ifdef DEBUG_EXEC_PATH
mtl_info("Copy");
#endif
		if (!(ops_ & (MessageType)Message::DontTryAtomicMove)) {
			if (TryAtomicMove()) {
#ifdef DEBUG_EXEC_PATH
				mtl_info("Atomic move succeeded");
#endif
				data().ChangeState(io::TaskState::Finished);
				return;
			}
		}
		CopyFiles();
	} else if (ops_ & (MessageType)Message::Move) {
#ifdef DEBUG_EXEC_PATH
		mtl_info("Move, trying to do atomic move..");
#endif
		if (TryAtomicMove()) {
#ifdef DEBUG_EXEC_PATH
mtl_info("Succeeded.");
#endif
			data().ChangeState(io::TaskState::Finished);
		} else {
#ifdef DEBUG_EXEC_PATH
mtl_info("Failed, doing manual copy/delete");
#endif
			CopyFiles();
			DeleteFiles();
		}
	} else {
#ifdef DEBUG_EXEC_PATH
mtl_info("Just copy");
#endif
		CopyFiles();
	}
	auto state = data_.GetState(nullptr);
	
	if (state & TaskState::Abort) {
		mtl_trace();
	} else {
		data_.ChangeState(TaskState::Finished);
	}
}

bool Task::TryAtomicMove()
{
	for (QString &path: file_paths_) {
		io::File *file = io::FileFromPath(path);
		if (file == nullptr)
			return false;
		
		QString new_path = to_dir_path_ + file->name();
		auto new_path_ba = new_path.toLocal8Bit();
		auto old_path_ba = file->build_full_path().toLocal8Bit();
		
		if (rename(old_path_ba.data(), new_path_ba.data()) != 0)
			return false;
	}
	
	return true;
}

int Task::TryCreateRegularFile(const QString &new_dir_path,
	const QString &filename, const int WriteFlags,
	const mode_t mode, const i64 file_size, QString &dest_path)
{
	const int OverwriteFlags = O_CREAT | O_TRUNC | O_LARGEFILE | O_NOFOLLOW | O_NOATIME | O_WRONLY;
	int file_flags = WriteFlags;
	int next = 0;
	dest_path = new_dir_path + io::NewNamePattern(filename, next);
	
	while (true)
	{
		data_.Lock();
		const auto file_exists_answer = data_.task_question_.file_exists_answer;
		const auto write_failed_answer = data_.task_question_.write_failed_answer;
		data_.Unlock();
		
		switch (file_exists_answer)
		{
		case FileExistsAnswer::None: break;
		case FileExistsAnswer::AutoRename: {
			data_.Lock();
			data_.task_question_.file_exists_answer = FileExistsAnswer::None;
			data_.Unlock();
			dest_path = new_dir_path + io::NewNamePattern(filename, next);
			break;
		}
		case FileExistsAnswer::AutoRenameAll: {
			dest_path = new_dir_path + io::NewNamePattern(filename, next);
			break;
		}
		case FileExistsAnswer::Overwrite: {
			data_.Lock();
			data_.task_question_.file_exists_answer = FileExistsAnswer::None;
			data_.Unlock();
			file_flags = OverwriteFlags;
			break;
		}
		case FileExistsAnswer::OverwriteAll: {
			file_flags = OverwriteFlags;
			break;
		}
		default: {
			mtl_trace();
			return -1;
		}
		} /// switch()
		
		auto dest_ba = dest_path.toLocal8Bit();
		const int fd = ::open(dest_ba.data(), file_flags, mode);
		if (fd != -1)
			return fd;
		
		if (errno == EEXIST)
		{
			if (file_exists_answer == FileExistsAnswer::SkipAll)
				return -1;
			
			TaskQuestion question = {};
			question.explanation = QObject::tr("File exists");
			question.file_path_in_question = dest_path;
			question.question = io::Question::FileExists;
			data_.ChangeState(TaskState::AwatingAnswer, &question);
			ActUponAnswer aua = DealWithFileExistsAnswer(file_size);
			if (aua == ActUponAnswer::Retry)
				continue;
			else if (aua == ActUponAnswer::Skip)
				return -1;
			else if (aua == ActUponAnswer::Abort)
				return -1;
			else {
				mtl_trace();
				return -1;
			}
		} else {
			if (write_failed_answer == WriteFailedAnswer::SkipAll)
				return -1;
			
			TaskQuestion question = {};
			question.explanation = strerror(errno);
			question.file_path_in_question = dest_path;
			question.question = io::Question::WriteFailed;
			data_.ChangeState(TaskState::AwatingAnswer, &question);
			ActUponAnswer answer = DealWithWriteFailedAnswer(file_size);
			switch(answer)
			{
			case ActUponAnswer::Retry: { break; }
			case ActUponAnswer::Skip: { return -1; }
			case ActUponAnswer::Abort: { return -1; }
			default: {mtl_trace(); return -1;}
			} /// switch()
		}
	}
}

ActUponAnswer
Task::DealWithFileExistsAnswer(const i64 file_size)
{
/** enum class FileExistsAnswer: i8 {
		None = 0,
		Overwrite,
		OverwriteAll,
		AutoRename,
		AutoRenameAll,
		Skip,
		SkipAll,
		Abort
	}; */
	if (!data_.WaitFor(TaskState::Answered)) {
		data_.ChangeState(TaskState::Error);
		return ActUponAnswer::Abort;
	}

	data_.Lock();
	const auto answer = data_.task_question_.file_exists_answer;
	data_.Unlock();
	
	switch (answer) {
	case FileExistsAnswer::Overwrite: {
		
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::OverwriteAll: {
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::AutoRename: {
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::AutoRenameAll: {
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::Skip: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.Lock();
		data_.task_question_.file_exists_answer = FileExistsAnswer::None;
		data_.Unlock();
		return ActUponAnswer::Skip;
	};
	case FileExistsAnswer::SkipAll: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return ActUponAnswer::Skip;
	}
	case FileExistsAnswer::Abort: {
		data_.ChangeState(TaskState::Abort);
		return ActUponAnswer::Abort;
	}
	default: {
		mtl_trace();
		return ActUponAnswer::Abort;
	}
	}
}

ActUponAnswer
Task::DealWithWriteFailedAnswer(const i64 file_size)
{
/** enum class WriteFailedAnswer: i8 {
		None = 0,
		Retry,
		Skip,
		SkipAll,
		Cancel,
	}; */
	if (!data_.WaitFor(TaskState::Answered)) {
		data_.ChangeState(TaskState::Error);
		return ActUponAnswer::Abort;
	}

	data_.Lock();
	const auto answer = data_.task_question_.write_failed_answer;
	data_.Unlock();
	
	switch (answer) {
	case WriteFailedAnswer::Skip: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.Lock();
		data_.task_question_.write_failed_answer = WriteFailedAnswer::None;
		data_.Unlock();
		return ActUponAnswer::Skip;
	};
	case WriteFailedAnswer::SkipAll: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return ActUponAnswer::Skip;
	}
	case WriteFailedAnswer::Retry: {
		data_.Lock();
		data_.task_question_.write_failed_answer = WriteFailedAnswer::None;
		data_.Unlock();
		return ActUponAnswer::Retry;
	}
	case WriteFailedAnswer::Abort: {
		data_.ChangeState(TaskState::Abort);
		return ActUponAnswer::Abort;
	}
	default: {
		mtl_trace();
		return ActUponAnswer::Abort;
	}
	}
}

} // namespace
