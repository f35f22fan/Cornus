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

//#define CORNUS_THROTTLE_IO

namespace cornus::io {

void TaskData::ChangeState(const TaskState new_state,
	TaskQuestion *task_question)
{
	auto g = cm.guard();
	if (task_question != nullptr)
		task_question_ = *task_question;
	const auto StopRecordingTimeFor = TaskState::Finished | TaskState::Pause
	| TaskState::Abort | TaskState::AwaitingAnswer;
	
	state = new_state;
	if (new_state & StopRecordingTimeFor)
	{
		work_time_recorder_.Pause();
	} else if (new_state & (TaskState::Continue | TaskState::Answered)) {
		work_time_recorder_.Continue();
	}
	
	cm.Broadcast();
}

void TaskData::RemoveBits(const TaskState states)
{
	state = static_cast<TaskState>(
	static_cast<TaskStateT>(state) & static_cast<TaskStateT>(~states));
}

TaskState TaskData::WaitFor(const TaskState new_state, const Lock l)
{
	auto g = cm.guard(l);
	
	while (!(state & new_state))
	{
		cm.CondWait();
	}
	
	if (new_state & TaskState::Continue)
		RemoveBits(TaskState::Continue);
	
	return state;
}

Task::Task() {}

Task::~Task() {}

void Task::CopyFiles()
{
	i64 total_size = CountTotalSize();
	if (total_size < 0)
		return;
	
	progress_.AddProgress(0, 0, &total_size);
	for (const auto &path: file_paths_)
	{
		CopyFileToDir(path, to_dir_path_);
	}
}

void Task::CopyFileToDir(const QString &file_path, const QString &in_dir_path)
{
	const QStringRef file_name = io::GetFileNameOfFullPath(file_path);
	if (file_name.isEmpty())
	{
		data_.ChangeState(TaskState::Abort);
		return;
	}
	/// PROGRESS>>
	progress_.SetDetails(file_name.toString());
	/// PROGRESS<<
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto file_ba = file_path.toLocal8Bit();
	
	if (statx(0, file_ba.data(), flags, fields, &stx_) != 0)
	{
		mtl_warn("statx(): %s: %s", strerror(errno), file_ba.data());
		data_.ChangeState(TaskState::Abort);
		return;
	}
	
	const i64 file_size = stx_.stx_size;
	const auto mode = stx_.stx_mode;
	i64 time_worked = 0;
	
	QString new_dir_path = in_dir_path;
	if (!new_dir_path.endsWith('/'))
		new_dir_path.append('/');
	if (S_ISDIR(mode))
	{
		QVector<QString> names;
		if (!io::ListFileNames(file_path, names)) {
			data_.ChangeState(TaskState::Abort);
			return;
		}
		
		new_dir_path.append(file_name);
		auto dir_ba = new_dir_path.toLocal8Bit();
		
		int status = mkdir(dir_ba.data(), mode);
		if (status != 0) {
			if (errno != EEXIST) {
				mtl_status(errno);
				data_.ChangeState(TaskState::Abort);
				return;
			}
		}
		auto state = data_.GetState(nullptr, &time_worked);
		Q_UNUSED(state);
		progress_.AddProgress(file_size, time_worked);
		
		for (const auto &name: names)
		{
			CopyFileToDir(file_path + '/' + name, new_dir_path);
			if (data_.GetState() & TaskState::Abort)
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
				data_.ChangeState(TaskState::Abort);
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
	const auto from_ba = from_path.toLocal8Bit();
	int input_fd = ::open(from_ba.data(), O_RDONLY | O_LARGEFILE);
	if (input_fd == -1)
	{
		mtl_warn("%s: %s", from_ba.data(), strerror(errno));
		data_.ChangeState(TaskState::Abort);
		return;
	}
	
	AutoCloseFd input_ac(input_fd);
	const int WarnIfExistsFlags = O_CREAT | O_EXCL | O_LARGEFILE
		| O_NOFOLLOW | O_NOATIME | O_WRONLY;
	QString dest_path;
	const int out_fd = TryCreateRegularFile(new_dir_path, filename,
		WarnIfExistsFlags, mode, file_size, dest_path);
	if (out_fd == -1)
	{
		mtl_trace("out_fd == -1");
		return;
	}
	
	AutoCloseFd output_ac(out_fd);
	loff_t in_off = 0, out_off = 0;
	const usize max_chunk = 512 * 128;
	i64 time_worked;
	
#ifdef CORNUS_THROTTLE_IO
	struct timespec throttle_ts;
	throttle_ts.tv_sec = 0;
	throttle_ts.tv_nsec = 300l * 1000000l;
#endif
	while (true)
	{
		const isize count = copy_file_range(input_fd, &in_off, out_fd, &out_off, max_chunk, 0);
		if (count == 0)
			break; // finished copying
		if (count == -1)
		{
			if (errno == EAGAIN)
				continue;
			
			data_.cm.Lock();
			const auto write_failed_answer = data_.task_question_.write_failed_answer;
			data_.cm.Unlock();
			
			if (write_failed_answer == WriteFailedAnswer::SkipAll) {
				mtl_info("Skip All");
				return;
			}
			
			TaskQuestion question = {};
			question.explanation = strerror(errno);
			question.file_path_in_question = dest_path;
			question.question = io::Question::WriteFailed;
			data_.ChangeState(TaskState::AwaitingAnswer, &question);
			ActUponAnswer answer = DealWithWriteFailedAnswer(file_size);
			if (answer == ActUponAnswer::Retry) {
				continue;
			} else if (answer == ActUponAnswer::Skip) {
				return;
			} else if (answer == ActUponAnswer::Abort) {
				return;
			}
			return;
		}
		
		auto state = data_.GetState(nullptr, &time_worked);
		progress_.AddProgress(count, time_worked);
		if (state & TaskState::Pause)
		{
			state = data_.WaitFor(TaskState::Continue | TaskState::Working | TaskState::Abort);
		}
		
		if (state & TaskState::Abort)
		{
			return;
		}
		
#ifdef CORNUS_THROTTLE_IO
		clock_nanosleep(CLOCK_REALTIME, 0, &throttle_ts, NULL);
#endif
	}
	CopyXAttr(input_fd, out_fd);
}

void Task::CopyXAttr(const int input_fd, const int output_fd)
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
	MTL_CHECK_VOID(buf != nullptr);
	
	AutoFree af(buf);
	/// Copy the list of attribute keys to the buffer.
	buflen = flistxattr(input_fd, buf, buflen);
	MTL_CHECK_VOID(buflen != -1);
	
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
//			if (errno != EPERM)
//				mtl_status(errno);
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

void Task::DeleteFiles()
{
	QString problematic_file;
	int status;
	for (const auto &path: file_paths_)
	{
		for(;;)
		{
			status = DeleteFile(path, stx_, problematic_file);
			if (status == 0)
				break;
			
			data_.cm.Lock();
			const auto delete_failed_answer = data_.task_question_.delete_failed_answer;
			data_.cm.Unlock();
			
			if (delete_failed_answer == DeleteFailedAnswer::SkipAll) {
				break;
			}
			
			TaskQuestion question = {};
			question.explanation = strerror(status);
			question.file_path_in_question = path;
			question.question = io::Question::DeleteFailed;
			mtl_info("Changing state to AwaitingAnswer");
			data_.ChangeState(TaskState::AwaitingAnswer, &question);
			mtl_info("Changing state to AwaitingAnswer ... Done");
			ActUponAnswer answer = DealWithDeleteFailedAnswer(stx_.stx_size);
			if (answer == ActUponAnswer::Retry) {
				mtl_trace("Retry");
				continue;
			} else if (answer == ActUponAnswer::Skip) {
				mtl_trace("Skip");
				break;
			} else if (answer == ActUponAnswer::Abort) {
				mtl_trace("Abort");
				return;
			} else {
				mtl_info("Unhandled answer: %d", (int)answer);
			}
		}
	}
}

int Task::DeleteFile(const QString &full_path, struct statx &stx,
	QString &problematic_file)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto ba = full_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
	{
		problematic_file = full_path;
		return errno;
	}
	
	if (S_ISDIR(stx.stx_mode))
	{
		QString dir_path = full_path;
		if (!dir_path.endsWith('/'))
			dir_path.append('/');
		
		QVector<QString> names;
		io::ListFileNames(dir_path, names);
		int status;
		for (const auto &name: names)
		{
			auto fp = dir_path + name;
			status = DeleteFile(fp, stx, problematic_file);
			if (status != 0)
				return status;
		}
	}
	
	if (remove(ba.data()) == 0)
		return 0;
	
	problematic_file = full_path;
	return errno;
}

Task* Task::From(cornus::ByteArray &ba, const HasSecret hs)
{
	if (hs == HasSecret::Yes)
		ba.next_u64();
	
	auto *task = new Task();
	task->ops_ = ba.next_u32();
	
	if (task->ops_ != (io::MessageType)io::Message::DeleteFiles &&
		task->ops_ != (io::MessageType)io::Message::MoveToTrash)
	{
		task->to_dir_path_ = ba.next_string();
		//mtl_printq(task->to_dir_path_);
	}
	
	while(ba.has_more())
	{
		QString s = ba.next_string();
		task->file_paths_.append(s);
		//mtl_printq(s);
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
		const int status = rename(old_path.data(), new_path.data());
		if (status != 0) {
			mtl_status(errno);
			continue;
		}
	}
}

void Task::SetDefaultAction(const IOAction action)
{
	auto &answer = data_.task_question_.file_exists_answer;
	switch (action) {
	case IOAction::AutoRenameAll: {
		answer = FileExistsAnswer::AutoRenameAll;
		break;
	}
	case IOAction::OverwriteAll: {
		answer = FileExistsAnswer::OverwriteAll;
		break;
	}
	case IOAction::SkipAll: {
		answer = FileExistsAnswer::SkipAll;
		break;
	}
	default: {
		mtl_warn();
		break;
	}
	}
}

void Task::StartIO()
{
	data_.ChangeState(TaskState::Continue | TaskState::Working);
	if (to_dir_path_.startsWith('/') && !to_dir_path_.endsWith('/'))
		to_dir_path_.append('/');
	if (file_paths_.isEmpty())
	{
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	const QString &first = file_paths_[0];
	auto name = io::GetFileNameOfFullPath(first);
	QString parent = first.mid(0, first.size() - name.size());
	const bool pasted = (int)ops_ & (int)Message::Pasted_Hint;
	//mtl_info("pasted: %d", pasted);
	if (name.isEmpty() || (!pasted && io::SameFiles(parent, to_dir_path_)))
	{
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	
	if (ops_ == (MessageType)Message::DeleteFiles) {
		DeleteFiles();
	} else if (ops_ == (MessageType)Message::MoveToTrash) {
		MoveToTrash();
	} else if (ops_ & (MessageType)Message::Copy) {
		const bool can_try_atomic_move = !(ops_ & (MessageType)Message::DontTryAtomicMove);
		if (can_try_atomic_move && TryAtomicMove()) {
			data().ChangeState(io::TaskState::Finished);
			return;
		}
		CopyFiles();
	} else if (ops_ & (MessageType)Message::Move) {
		if (TryAtomicMove())
		{
			data().ChangeState(io::TaskState::Finished);
		} else {
			CopyFiles();
			const auto state = data_.GetState();
			if (!(state & TaskState::Abort))
			{
				DeleteFiles();
			}
		}
	} else {
		CopyFiles();
	}
	
	const bool has_abort = data_.GetState() & TaskState::Abort;
	if (!has_abort) {
		mtl_trace("Doesn't have abort");
		data_.ChangeState(TaskState::Finished);
	} else {
		mtl_info("Does have abort");
	}
	
	mtl_info("Returning from Task::StartIO()");
}

bool Task::TryAtomicMove()
{
	for (QString &path: file_paths_)
	{
		io::File *file = io::FileFromPath(path);
		MTL_CHECK(file != nullptr);
		
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
	dest_path = new_dir_path + filename;
	
	while (true)
	{
		auto dest_ba = dest_path.toLocal8Bit();
		const int fd = ::open(dest_ba.data(), file_flags, mode);
		if (fd != -1)
		{
			mtl_info("SUCCESS");
			return fd;
		}
		const int Error = errno;
		mtl_status(Error);
		bool go_to_loop_start = false;
		if (Error == EEXIST)
		{
			data_.cm.Lock();
			const auto file_exists_answer = data_.task_question_.file_exists_answer;
			data_.cm.Unlock();
			
			switch (file_exists_answer) {
			case FileExistsAnswer::None: {
				mtl_trace();
				break;
			}
			case FileExistsAnswer::AutoRename: {
				mtl_trace();
				data_.cm.Lock();
				data_.task_question_.file_exists_answer = FileExistsAnswer::None;
				data_.cm.Unlock();
				return io::CreateAutoRenamedFile(new_dir_path, filename, file_flags, mode);
			}
			case FileExistsAnswer::AutoRenameAll: {
				mtl_trace();
				return io::CreateAutoRenamedFile(new_dir_path, filename, file_flags, mode);
			}
			case FileExistsAnswer::Overwrite: {
				mtl_trace();
				data_.cm.Lock();
				data_.task_question_.file_exists_answer = FileExistsAnswer::None;
				data_.cm.Unlock();
				file_flags = OverwriteFlags;
				go_to_loop_start = true;
				break;
			}
			case FileExistsAnswer::OverwriteAll: {
				mtl_trace();
				file_flags = OverwriteFlags;
				go_to_loop_start = true;
				break;
			}
			case FileExistsAnswer::SkipAll: {
				mtl_trace();
				return -1;
			}
			default: {
				mtl_trace();
				return -1;
			}
			} // switch()
			
			if (go_to_loop_start)
				continue;
			TaskQuestion question = {};
			question.explanation = QObject::tr("File exists");
			question.file_path_in_question = dest_path;
			question.question = io::Question::FileExists;
mtl_trace("Before changing state");
			data_.ChangeState(TaskState::AwaitingAnswer, &question);
mtl_info("After changing state, waiting for reply");
			ActUponAnswer answer = DealWithFileExistsAnswer(file_size);
mtl_info("Got the answer");
			switch (answer) {
			case ActUponAnswer::Retry: {
mtl_trace("ActUponAnswer::Retry");
				break;
			}
			case ActUponAnswer::Skip: {
mtl_trace("ActUponAnswer::Skip");
				return -1;
			}
			case ActUponAnswer::Abort: {
mtl_trace("ActUponAnswer::Abort");
				return -1;
			}
			default: {
				return -1;
			}
			}
		}
	}

mtl_trace();
}

ActUponAnswer
Task::DealWithDeleteFailedAnswer(const i64 file_size)
{
/** enum class DeleteFailedAnswer: i8 {
	None = 0,
	Retry,
	Skip,
	SkipAll,
	Abort
};
	}; */
	data_.WaitFor(TaskState::Answered);
	data_.cm.Lock();
	const auto answer = data_.task_question_.delete_failed_answer;
	data_.cm.Unlock();
	
	switch (answer) {
	case DeleteFailedAnswer::Skip: {
mtl_trace();
		const i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.cm.Lock();
		data_.task_question_.delete_failed_answer = DeleteFailedAnswer::None;
		data_.cm.Unlock();
mtl_trace();
		return ActUponAnswer::Skip;
	}
	case DeleteFailedAnswer::SkipAll: {
		const i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return ActUponAnswer::Skip;
	}
	case DeleteFailedAnswer::Retry: {
mtl_trace();
		data_.cm.Lock();
		data_.task_question_.delete_failed_answer = DeleteFailedAnswer::None;
		data_.cm.Unlock();
mtl_trace();
		return ActUponAnswer::Retry;
	}
	case DeleteFailedAnswer::Abort: {
mtl_trace("Abort");
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
	data_.WaitFor(TaskState::Answered);
	data_.cm.Lock();
	const auto answer = data_.task_question_.file_exists_answer;
	data_.cm.Unlock();
	
	switch (answer) {
	case FileExistsAnswer::Overwrite: {
mtl_trace();
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::OverwriteAll: {
mtl_trace();
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::AutoRename: {
mtl_trace();
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::AutoRenameAll: {
mtl_trace();
		return ActUponAnswer::Retry;
	}
	case FileExistsAnswer::Skip: {
mtl_trace();
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.cm.Lock();
		data_.task_question_.file_exists_answer = FileExistsAnswer::None;
		data_.cm.Unlock();
mtl_trace();
		return ActUponAnswer::Skip;
	};
	case FileExistsAnswer::SkipAll: {
mtl_trace();
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return ActUponAnswer::Skip;
	}
	case FileExistsAnswer::Abort: {
mtl_trace();
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
	Abort,
}; */
	data_.WaitFor(TaskState::Answered);
	data_.cm.Lock();
	const auto answer = data_.task_question_.write_failed_answer;
	data_.cm.Unlock();
	
	switch (answer) {
	case WriteFailedAnswer::Skip: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.cm.Lock();
		data_.task_question_.write_failed_answer = WriteFailedAnswer::None;
		data_.cm.Unlock();
		return ActUponAnswer::Skip;
	};
	case WriteFailedAnswer::SkipAll: {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return ActUponAnswer::Skip;
	}
	case WriteFailedAnswer::Retry: {
		data_.cm.Lock();
		data_.task_question_.write_failed_answer = WriteFailedAnswer::None;
		data_.cm.Unlock();
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
