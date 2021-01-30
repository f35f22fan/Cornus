#include "Task.hpp"

#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "File.hpp"
#include "io.hh"

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE /// for copy_file_range() as of Linux 5.3+
#endif
#include <unistd.h>

namespace cornus::io {

bool TaskData::ChangeState(const TaskState new_state)
{
	MutexGuard guard(&mutex);
	
	state = new_state;
	int status = pthread_cond_broadcast(&cond);
	if (status != 0) {
		mtl_status(status);
		return false;
	}
	
	return true;
}

bool
TaskData::WaitFor(const TaskState new_state)
{
	if (!Lock())
		return false;
	
	bool succeeded = true;
	
	while (state != new_state) {
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

Task::Task() {
	progress_.data.time_created = time(NULL);
}
Task::~Task() {}

void
Task::CopyFiles()
{
	i64 total_size = CountTotalSize();
	if (total_size < 0)
		return;
	
	progress_.AddProgress(0, &total_size);
	
	for (const auto &path: file_paths_) {
		CopyFileToDir(path, to_dir_path_);
	}
	
	data_.ChangeState(TaskState::Finished);
}

void
Task::CopyFileToDir(const QString &file_path, const QString &dir_path)
{
	progress_.SetDetails(file_path);
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto file_ba = file_path.toLocal8Bit();
	
	if (statx(0, file_ba.data(), flags, fields, &stx_) != 0) {
		mtl_warn("statx(): %s: %s", strerror(errno), file_ba.data());
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	const i64 size = stx_.stx_size;
	const auto mode = stx_.stx_mode;
	
	int last_slash_index = file_path.lastIndexOf('/');
	if (last_slash_index == -1) {
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	const QStringRef file_name = file_path.midRef(last_slash_index);
	
	if (S_ISDIR(mode)) {
		
		QVector<QString> names;
		if (io::ListFileNames(file_path, names) != io::Err::Ok) {
			data_.ChangeState(TaskState::Error);
			return;
		}
		
		QString new_dir_path = dir_path + file_name;
		auto dir_ba = new_dir_path.toLocal8Bit();
		
		int status = mkdir(dir_ba.data(), mode);
		if (status != 0) {
			mtl_status(errno);
			data_.ChangeState(TaskState::Error);
			return;
		}
		
		progress_.AddProgress(size);
		
		for (const auto &name: names)
		{
			CopyFileToDir(file_path + '/' + name, new_dir_path);
			if (data_.GetState() == TaskState::Error)
				return;
		}
	} else if (S_ISREG(mode)) {
		CopyRegularFile(file_path, dir_path + file_name, mode, size);
	} else if (S_ISLNK(mode)) {
		
		QString link_target_path;
		if (ReadLinkSimple(file_ba.data(), link_target_path)) {
			auto target_path_ba = link_target_path.toLocal8Bit();
			auto new_file_path = (dir_path + file_name).toLocal8Bit();
			int status = symlink(target_path_ba.data(), new_file_path.data());
			
			if (status != 0)
			{
				mtl_status(errno);
				mtl_printq2("target: ", link_target_path);
				mtl_printq2("new_file: ", (dir_path + file_name));
				data_.ChangeState(TaskState::Error);
				return;
			}
			progress_.AddProgress(size);
		}
	} else {
		mtl_trace("fifos/pipes/sockets/block devices not copied: %s",
			file_ba.data());
	}
}

void
Task::CopyRegularFile(const QString &from_path, const QString &dest_path,
	const mode_t mode, const i64 file_size)
{
	auto from_ba = from_path.toLocal8Bit();
	int input_fd = ::open(from_ba.data(), O_RDONLY | O_NOATIME | O_LARGEFILE);
	
	if (input_fd == -1) {
		mtl_status(errno);
		data_.ChangeState(TaskState::Error);
		return;
	}
	AutoCloseFd input_ac(input_fd);
	
	auto dest_ba = dest_path.toLocal8Bit();
	const auto WriteFlags = O_CREAT | O_EXCL | O_LARGEFILE | O_NOATIME | O_WRONLY;
	int output_fd = ::open(dest_ba, WriteFlags, mode);
	if (output_fd == -1) {
		mtl_status(errno);
		data_.ChangeState(TaskState::Error);
		return;
	}
	
	AutoCloseFd output_ac(output_fd);
	i64 so_far = 0;
	//off_t offset = 0;
	loff_t off = 0;
	const usize chunk = 256 * 1024;
	
	while (so_far < file_size) {
		//isize count = sendfile(output_fd, input_fd, &offset, chunk);
		isize count = copy_file_range(input_fd, &off, output_fd, &off, chunk, 0);
		if (count == -1) {
			if (errno == EAGAIN)
				continue;
			data_.ChangeState(TaskState::Error);
			return;
		} else if (count == 0) {
			break;
		}
		
		so_far += count;
	}
}

i64
Task::CountTotalSize()
{
	i64 size = 0;
	for (const auto &path: file_paths_)
	{
		i64 sz = io::CountSizeRecursive(path, stx_);
		if (sz < 0) {
			mtl_printq(path);
			return -1;
		}
		size += sz;
	}
	
	return size;
}

Task*
Task::From(cornus::ByteArray &ba)
{
	auto *task = new Task();
	task->ops_ = ba.next_u32();
	//mtl_info("ops: %u", task->ops_);
	task->to_dir_path_ = ba.next_string();
	const i32 count = ba.next_i32();
	
	for (int i = 0; i < count; i++) {
		task->file_paths_.append(ba.next_string());
	}
	
	return task;
}

void
Task::StartIO()
{
	if (!to_dir_path_.isEmpty() && !to_dir_path_.endsWith('/'))
		to_dir_path_.append('/');
	
	if (ops_ & io::socket::MsgBits::AtomicMove) {
		if (TryAtomicMove()) {
			data().ChangeState(io::TaskState::Finished);
			return;
		}
	}
	if (ops_ & io::socket::MsgBits::Copy) {
		CopyFiles();
	} else {
		mtl_trace();
	}
	
	
	///data().ChangeState(io::TaskState::Finished);
}

bool
Task::TryAtomicMove()
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

//void
//Task::WaitForStartSignal()
//{
//	mtl_info("waiting for start signal (continue)");
//	if (data().WaitFor(io::TaskState::Continue)) {
//		mtl_info("Done! - Starting I/O!");
//		StartIO();
//	} else {
//		mtl_trace();
//	}
//}

}
