#include "Task.hpp"

#include "../ByteArray.hpp"

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

Task::Task() {}
Task::~Task() {}

Task*
Task::From(cornus::ByteArray &ba)
{
	auto *task = new Task();
	task->ops_ = ba.next_u32();
	task->to_dir_path_ = ba.next_string();
	const i32 count = ba.next_i32();
	
	for (int i = 0; i < count; i++) {
		task->file_paths_.append(ba.next_string());
	}
	
	return task;
}

void
Task::StartIO() {
	mtl_info("IO started");
	data().ChangeState(io::TaskState::Finished);
}

void
Task::WaitForStartSignal() {
	mtl_info("waiting for start signal (continue)");
	if (data().WaitFor(io::TaskState::Continue)) {
		mtl_info("Done! - Starting I/O!");
		StartIO();
	} else {
		mtl_trace();
	}
}

}
