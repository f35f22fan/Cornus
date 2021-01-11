#pragma once

#include "err.hpp"
#include <pthread.h>

namespace cornus {

class MutexGuard {
public:
	MutexGuard(pthread_mutex_t *mutex);
	~MutexGuard();
	
	bool Signal(pthread_cond_t *cond);
	
private:
	NO_ASSIGN_COPY_MOVE(MutexGuard);
	pthread_mutex_t *mutex_ = nullptr;
};

}
