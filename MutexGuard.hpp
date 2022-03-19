#pragma once

#include "err.hpp"
#include <pthread.h>

namespace cornus {

enum class LockType: i1 {
	Normal,
	TryLock
};

class MutexGuard {
public:
	MutexGuard() {}
	MutexGuard(pthread_mutex_t *mutex, const LockType lock_type = LockType::Normal);
	virtual ~MutexGuard();
	
	bool Signal(pthread_cond_t *cond);
	
private:
	pthread_mutex_t *mutex_ = nullptr;
};

}
