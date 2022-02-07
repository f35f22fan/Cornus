#include "MutexGuard.hpp"

namespace cornus {

MutexGuard::MutexGuard(pthread_mutex_t *mutex, const LockType lock_type):
mutex_(mutex)
{
	switch(lock_type) {
	case LockType::Normal:
	{
		const int status = pthread_mutex_lock(mutex_);
		if (status != 0)
			mtl_status(status);
		break;
	}
	case LockType::TryLock: {
		const int status = pthread_mutex_trylock(mutex_);
		if (status != 0)
			mutex_ = nullptr;
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

MutexGuard::~MutexGuard()
{
	if (mutex_ != nullptr)
	{
		const int status = pthread_mutex_unlock(mutex_);
		if (status != 0)
			mtl_status(status);
	}
}

bool MutexGuard::Signal(pthread_cond_t *cond) {
	int status = pthread_cond_signal(cond);
	if (status != 0)
		mtl_status(status);
	return status == 0;
}

}
