#pragma once

#include "err.hpp"
#include <pthread.h>

namespace cornus {

enum class LockType: i8 {
	Normal,
	TryLock
};

class MutexGuard {
public:
	MutexGuard(pthread_mutex_t *mutex, const LockType lock_type = LockType::Normal);
	~MutexGuard();
	
	bool Signal(pthread_cond_t *cond);
	
private:
	pthread_mutex_t *mutex_ = nullptr;
};

}
