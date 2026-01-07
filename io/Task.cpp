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

cbool ThrottleIO = false;
cint OverwriteFlags = O_TRUNC | O_LARGEFILE | O_NOFOLLOW | O_NOATIME | O_WRONLY;
int step = 0;

QString ToString(const io::TaskState state)
{
	QString s;
	if (state & io::TaskState::Abort)
		s.append("Abort|");
	if (state & io::TaskState::Answered)
		s.append("Answered|");
	if (state & io::TaskState::AwaitingAnswer)
		s.append("AwaitingAnswer|");
	if (state & io::TaskState::Continue)
		s.append("Continue|");
	if (state & io::TaskState::Finished)
		s.append("Finished|");
	if (state & io::TaskState::Pause)
		s.append("Pause|");
	if (state & io::TaskState::Working)
		s.append("Working|");
	
	return s;
}

void TaskData::ChangeState(const TaskState new_state, Answer *answer, TaskQuestion *question)
{
	auto g = cm.guard();
	if (answer != nullptr)
		answer_ = *answer;
	if (question != nullptr) {
		task_question_ = *question;
	}
	cauto StopRecordingTime = TaskState::Finished | TaskState::Pause
	| TaskState::Abort | TaskState::AwaitingAnswer;
	
	// if (new_state & TaskState::Working) {
		mtl_info("%d CHANGED STATE TO: %s", step++, qPrintable(ToString(new_state)));
	// }
	
	state = new_state;
	if (new_state & StopRecordingTime)
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
	CountTotalSize();
	for (cauto &path: file_paths_)
	{
		mtl_info("Copy \"%s\" to \"%s\"", qPrintable(path),
			qPrintable(to_dir_path_));
		CopyFileToDir(path, to_dir_path_);
	}
}

void Task::CopyFileToDir(const QString &file_path, const QString &in_dir_path)
{
	auto file_name = io::GetFileNameOfFullPath(file_path);
	if (file_name.isEmpty())
	{
		data_.ChangeState(TaskState::Abort);
		return;
	}
	/// PROGRESS>>
	progress_.SetDetails(file_name.toString());
	/// PROGRESS<<
	cauto flags = AT_SYMLINK_NOFOLLOW;
	cauto fields = STATX_SIZE | STATX_MODE;
	auto file_ba = file_path.toLocal8Bit();
	
	if (statx(0, file_ba.data(), flags, fields, &stx_) != 0)
	{
		mtl_warn("statx(): %s: %s", strerror(errno), file_ba.data());
		data_.ChangeState(TaskState::Abort);
		return;
	}
	
	ci64 file_size = stx_.stx_size;
	cauto mode = stx_.stx_mode;
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
		cint status = mkdir(dir_ba.data(), mode);
		if (status != 0)
		{
			if (errno != EEXIST)
			{
				mtl_status(errno);
				data_.ChangeState(TaskState::Abort);
				return;
			}
		}
		auto state = data_.GetState(nullptr, &time_worked);
		Q_UNUSED(state);
		progress_.AddProgress(file_size, time_worked);
		
		for (cauto &name: names)
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
			auto new_file_path = (new_dir_path + file_name.toString()).toLocal8Bit();
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
	const QString &filename, const mode_t mode, ci64 file_size)
{
	cauto from_ba = from_path.toLocal8Bit();
	int input_fd = ::open(from_ba.data(), O_RDONLY | O_LARGEFILE);
	if (input_fd == -1)
	{
		mtl_warn("%s: %s", from_ba.data(), strerror(errno));
		data_.ChangeState(TaskState::Abort);
		return;
	}
	
	AutoCloseFd input_ac(input_fd);
	cint WarnIfExistsFlags = O_CREAT | O_EXCL | O_LARGEFILE
		| O_NOFOLLOW | O_NOATIME | O_WRONLY;
	QString dest_path;
	cint out_fd = TryCreateRegularFile(new_dir_path, filename,
		WarnIfExistsFlags, mode, file_size, dest_path);
	if (out_fd == -1)
	{
		mtl_trace("out_fd == -1");
		data_.ChangeState(TaskState::Abort);
		return;
	}
mtl_trace("=========================");
	data_.ChangeState(TaskState::Working|TaskState::Continue);
	AutoCloseFd output_ac(out_fd);
	loff_t in_off = 0, out_off = 0;
	const usize max_chunk = 512 * 128;
	i64 time_worked;
	struct timespec throttle_ts;
	if (ThrottleIO) {
		throttle_ts.tv_sec = 0;
		throttle_ts.tv_nsec = 300l * 1000000l;
	}
	cisize bufsize = 4096 * 16;
	char *buf = new char[bufsize];
	AutoDeleteArr buf_(buf);
	cbool use_copy_file_range = false;
	while (true)
	{
		isize read_amount;
		if (use_copy_file_range)
		{
			read_amount = copy_file_range(input_fd, &in_off, out_fd, &out_off, max_chunk, 0);
			
		} else {
			read_amount = ::read(input_fd, buf, bufsize);
			if (read_amount > 0)
			{
				isize written = 0;
				while (written < read_amount)
				{
					cisize wrote = ::write(out_fd, buf + written, read_amount - written);
					if (wrote  == -1)
					{
						if (errno == EAGAIN)
							continue;
						mtl_errno();
						read_amount = -1;
						break;
					}
					
					written += wrote;
				}
			}
		}
		
		if (read_amount == 0)
			break; // finished copying
		
		if (read_amount == -1)
		{
			if (errno == EAGAIN)
				continue;
mtl_warn("%s", strerror(errno));
			Answer answer = data_.GetAnswerWithLock();
			
			if (answer.skip_all()) {
				mtl_info("Skip All");
				return;
			}
			
			TaskQuestion question = {};
			question.explanation = strerror(errno);
			question.file_path_in_question = dest_path;
			question.question = io::Question::WriteFailed;
			data_.ChangeState(TaskState::AwaitingAnswer, 0, &question);
			Answer user_reply = WaitForWriteFailedAnswer(file_size);
			if (user_reply.retry()) {
				continue;
			} else if (user_reply.any_skip()) {
				return;
			} else if (user_reply.abort()) {
				return;
			}
			return;
		}
		
		auto state = data_.GetState(nullptr, &time_worked);
		progress_.AddProgress(read_amount, time_worked);
		if (state & TaskState::Pause)
		{
			state = data_.WaitFor(TaskState::Continue | TaskState::Working | TaskState::Abort);
		}
		
		if (state & TaskState::Abort)
		{
			return;
		}
		
		if (ThrottleIO) {
			clock_nanosleep(CLOCK_REALTIME, 0, &throttle_ts, NULL);
		}
	}
	CopyXAttr(input_fd, out_fd);
}

void Task::CopyXAttr(cint input_fd, cint output_fd)
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
	for (cauto &path: file_paths_)
	{
		if (!io::CountSizeRecursive(path, stx_, info)) {
			mtl_printq(path);
			return -1;
		}
	}
	
	ci64 total_progress = info.size + info.file_count + info.dir_count;
	progress_.AddProgress(0, 0, &total_progress);
	
	return info.size;
}

void Task::DeleteFiles(const InitTotalSize its)
{
	if (its == InitTotalSize::Yes)
		CountTotalSize();
	
	i64 time_worked = 0;
	QString problematic_file;
	for (cauto &path: file_paths_)
	{
		while(true)
		{
			cint status = DeleteFile(path, stx_, problematic_file);
			if (status == 0)
			{
				auto state = data_.GetState(nullptr, &time_worked);
				Q_UNUSED(state);
				progress_.AddProgress(1, time_worked);
				
				break;
			}
			
			data_.cm.Lock();
			const Answer delete_failed_answer = data_.answer_;
			data_.cm.Unlock();
			
			if (delete_failed_answer.skip_all()) {
				break;
			}
			
			TaskQuestion question = {};
			question.explanation = strerror(status);
			question.file_path_in_question = path;
			question.question = io::Question::DeleteFailed;
			data_.ChangeState(TaskState::AwaitingAnswer, 0, &question);
			Answer user_reply = WaitForDeleteFailedAnswer(stx_.stx_size);
			if (user_reply.retry())
			{
				continue;
			} else if (user_reply.skip()) {
				break;
			} else if (user_reply.abort()) {
				return;
			} else {
				mtl_info("Unhandled answer: %d", (int)user_reply.bits());
			}
		}
	}
}

int Task::DeleteFile(const QString &full_path, struct statx &stx,
	QString &problematic_file)
{
	cauto flags = AT_SYMLINK_NOFOLLOW;
	cauto fields = STATX_MODE; //STATX_SIZE | ;
	auto ba = full_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
	{
		problematic_file = full_path;
		return errno;
	}
	
	const bool is_dir = S_ISDIR(stx.stx_mode);
	if (is_dir)
	{
		QString dir_path = full_path;
		if (!dir_path.endsWith('/'))
			dir_path.append('/');
		
		QVector<QString> names;
		io::ListFileNames(dir_path, names);
		for (cauto &name: names)
		{
			cauto fp = dir_path + name;
			cint status = DeleteFile(fp, stx, problematic_file);
			if (status != 0)
				return status;
		}
	}
	
	cint status = is_dir ? rmdir(ba.data()) : unlink(ba.data());
	if (status == 0)
		return 0;
	
	problematic_file = full_path;
	return errno;
}

bool Task::FixDestDir()
{
	const DirType dt = io::GetDirType(to_dir_path_);
	
	if (dt == DirType::Error)
		return false;
	
	if (dt == DirType::Neither)
	{
		to_dir_path_ = io::GetParentDirPath(to_dir_path_).toString();
		return !to_dir_path_.isEmpty();
	}
	
	return true;
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
		QUrl url = QUrl::fromUserInput(s);
		task->urls_.append(url);
		task->file_paths_.append(url.toLocalFile());
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
	
	ci64 now = time(NULL);
	const QString time_str = trash::time_to_str(now) + '_';
	
	for (auto &next: file_paths_)
	{
		auto name = io::GetFileNameOfFullPath(next);
		auto new_path = (trash_path + time_str + name.toString()).toLocal8Bit();
		auto old_path = next.toLocal8Bit();
		//mtl_info("new_path: %s\n, old_path: %s", new_path.data(), old_path.data());
		cint status = rename(old_path.data(), new_path.data());
		if (status != 0) {
			mtl_status(errno);
			continue;
		}
	}
}

void Task::SetDefaultAction(const IOAction action)
{
	auto &answer = data_.answer_;//data_.task_question_.file_exists_answer;
	switch (action) {
	case IOAction::AutoRenameAll: {
		answer.auto_rename_all(true);
		break;
	}
	case IOAction::OverwriteAll: {
		answer.overwrite_all(true);
		break;
	}
	case IOAction::SkipAll: {
		answer.skip_all(true);
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
	if (file_paths_.isEmpty())
	{
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	
	data_.ChangeState(TaskState::Continue | TaskState::Working);
	if (to_dir_path_.startsWith('/') && !to_dir_path_.endsWith('/'))
		to_dir_path_.append('/');
	
	const QString &first_file_path = file_paths_[0];
	auto fn = io::GetFileNameOfFullPath(first_file_path);
	cauto parent_dir = io::GetParentDirPath(first_file_path).toString();
	const bool pasted = (int)ops_ & (int)Message::Pasted_Hint;
	//mtl_info("pasted: %d", pasted);
	//mtl_info("%s, %s", qPrintable(parent_dir), qPrintable(to_dir_path_));
	if (fn.isEmpty() || (!pasted && io::SameFiles(parent_dir, to_dir_path_)))
	{
		data().ChangeState(io::TaskState::Finished);
		return;
	}
	
	if (ops_ == (MessageType)Message::DeleteFiles)
	{
		DeleteFiles(InitTotalSize::Yes);
	} else if (ops_ == (MessageType)Message::MoveToTrash) {
		MoveToTrash();
	} else if (ops_ & (MessageType)Message::Copy) {
		if (!FixDestDir())
		{
			data().ChangeState(io::TaskState::Finished);
			return;
		}
		const bool can_try_atomic_move = !(ops_ & (MessageType)Message::DontTryAtomicMove);
		if (can_try_atomic_move && TryAtomicMove()) {
			data().ChangeState(io::TaskState::Finished);
			return;
		}
		CopyFiles();
	} else if (ops_ & (MessageType)Message::Move) {
		if (!FixDestDir())
		{
			data().ChangeState(io::TaskState::Finished);
			return;
		}
		if (TryAtomicMove())
		{
			data().ChangeState(io::TaskState::Finished);
		} else {
			CopyFiles();
			cauto state = data_.GetState();
			if (!(state & TaskState::Abort))
			{
				DeleteFiles(InitTotalSize::No);
			}
		}
	} else {
		if (!FixDestDir())
		{
			data().ChangeState(io::TaskState::Finished);
			return;
		}
		CopyFiles();
	}
	
	const bool has_abort = data_.GetState() & TaskState::Abort;
	if (!has_abort) {
		data_.ChangeState(TaskState::Finished);
	}
	
//	mtl_info("Returning from Task::StartIO()");
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
	const QString &filename, cint WriteFlags,
	const mode_t mode, ci64 file_size, QString &dest_path)
{
	int file_flags = WriteFlags;
	dest_path = new_dir_path + filename;
	int count = 0;
	
	while (true)
	{
		loop_start:
		count++;
			if (count > 10) {
			return -1;
			}
		auto dest_ba = dest_path.toLocal8Bit();
			mtl_info("file_flags: %o, OverwriteFlags: %o, original: %o", file_flags, OverwriteFlags, WriteFlags);
		cint fd = ::open(dest_ba.data(), file_flags, mode);
		if (fd != -1)
		{
			return fd;
		}
		cint error_code = errno;
		if (error_code == EEXIST)
		{
			Answer answer = WaitForFileExistsAnswer(new_dir_path, filename, dest_path, file_flags, file_size, mode);
			if (answer.retry()) {
				break;
			} else if (answer.skip()) {
				return -1;
			} else if (answer.skip_all()) {
				return -1;
			} else if (answer.abort()) {
				return -1;
			} else if (answer.retry_or_overwrite()) {
				goto loop_start;
			} else {
				mtl_trace();
				return -1;
			}
			
		} else if (error_code == EACCES) {
			mtl_warn("Don't have permission for %s", dest_ba.data());
			Answer answer = WaitForFileAccessAnswer(file_size, dest_path);
			if (answer.retry()) {
				mtl_trace("retry");
				continue;
			} else if (answer.skip()) {
				mtl_trace("skip");
				return -1;
			} else if (answer.skip_all()) {
				mtl_trace("skip all");
				return -1;
			} else if (answer.abort()) {
				mtl_trace("abort");
				return -1;
			} else {
				mtl_trace();
				return -1;
			}
		} else {
			mtl_warn("errno is: %d", error_code);
			return -1;
		}
	}
	
	mtl_trace();
	return -1;
}

Answer
Task::WaitForDeleteFailedAnswer(ci64 file_size)
{
	data_.WaitFor(TaskState::Answered);
	Answer answer = data_.GetAnswerWithLock();
	
	if (answer.skip()) {
		ci64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.cm.Lock();
		data_.answer_.clear();
		data_.cm.Unlock();

		return answer;
	} else if (answer.skip_all()) {
		ci64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return answer;
	} else if (answer.retry()) {
		data_.cm.Lock();
		data_.answer_.clear();
		data_.cm.Unlock();

		return answer;
	} else if (answer.abort()) {
		data_.ChangeState(TaskState::Abort);
		return answer;
	} else {
		answer.clear();
		answer.abort(true);
		return answer;
	}
}

Answer Task::WaitForFileExistsAnswer(QString dir_path, QString filename,
	QString dest_path, int &file_flags, ci64 file_size, mode_t mode) {
	Answer answer = data_.GetAnswerWithLock();
	
	while (answer.none()) {
		TaskQuestion question = {};
		question.explanation = QObject::tr("File exists");
		question.file_path_in_question = dest_path;
		question.question = io::Question::FileExists;
		data_.ChangeState(TaskState::AwaitingAnswer, 0, &question);
		data_.WaitFor(TaskState::Answered);
		answer = data_.GetAnswerWithLock();
	}
	
	if (answer.auto_rename()) {
		cint fd = io::CreateAutoRenamedFile(dir_path, filename, file_flags, mode);
		if (fd == -1) {
			answer = WaitForWriteFailedAnswer(file_size);
		} else {
			answer.retry(true);
		}
		answer.auto_rename(false);
		return answer;
	} else if (answer.auto_rename_all()) {
		cint fd = io::CreateAutoRenamedFile(dir_path, filename, file_flags, mode);
		if (fd == -1) {
			answer = WaitForWriteFailedAnswer(file_size);
		} else {
			answer.retry(true);
		}
		return answer;
	} else if (answer.overwrite()) {
		mtl_trace("overwritie");
		file_flags = OverwriteFlags;
		return answer;
	} else if (answer.overwrite_all()) {
		mtl_trace("overwrite all");
		file_flags = OverwriteFlags;
		return answer;
	} else if (answer.skip()) {
		mtl_trace("skip");
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return answer;
	} else if (answer.skip_all()) {
		mtl_trace("skip all");
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return answer;
	} else if (answer.abort()) {
		data_.ChangeState(TaskState::Abort);
		return answer;
	}
	
	mtl_trace();
	answer.clear();
	answer.abort(true);
	data_.ChangeState(TaskState::Abort);
	
	return answer;
}

Answer
Task::WaitForWriteFailedAnswer(ci64 file_size)
{
	data_.WaitFor(TaskState::Answered);
	Answer answer = data_.GetAnswerWithLock();
	if (answer.skip()){
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		data_.cm.Lock();
		data_.answer_.clear();
		data_.cm.Unlock();
		return answer;
	} else if (answer.skip_all()) {
		i64 time_worked = data_.GetTimeWorked();
		progress_.AddProgress(file_size, time_worked);
		return answer;
	} else if (answer.retry()) {
		data_.cm.Lock();
		data_.answer_.clear();
		data_.cm.Unlock();
		return answer;
	} else if (answer.abort()) {
		data_.ChangeState(TaskState::Abort);
		return answer;
	} else {
		answer.clear();
		answer.abort(true);
		data_.ChangeState(TaskState::Abort);
		return answer;
	}
}

Answer
Task::WaitForFileAccessAnswer(ci64 file_size, QString dest_path)
{
	TaskQuestion question = {};
	question.explanation = QObject::tr("File access");
	question.file_path_in_question = dest_path;
	question.question = io::Question::WriteFailed;
	data_.ChangeState(TaskState::AwaitingAnswer, 0, &question);
	data_.WaitFor(TaskState::Answered);
	Answer answer = data_.GetAnswerWithLock();
	
	if (answer.none()) {
		mtl_trace();
		answer.clear();
		answer.abort(true);
		data_.ChangeState(TaskState::Abort);
		return answer;
	} else if (answer.retry()) {
		mtl_trace("retry");
		return answer;
	} else if (answer.skip()) {
		mtl_trace("skip");
		return answer;
	} else if (answer.skip_all()) {
		mtl_trace("skip all");
		return answer;
	} else {
		mtl_trace("other");
		answer.clear();
		answer.abort(true);
		data_.ChangeState(TaskState::Abort);
		return answer;
	}
}

} // namespace
