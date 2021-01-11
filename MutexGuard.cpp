#include "MutexGuard.hpp"

namespace cornus {

MutexGuard::MutexGuard(pthread_mutex_t *mutex): mutex_(mutex)
{
	int status = pthread_mutex_lock(mutex_);
	if (status != 0)
		mtl_status(status);
}

MutexGuard::~MutexGuard()
{
	if (mutex_ != nullptr) {
		int status = pthread_mutex_unlock(mutex_);
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
