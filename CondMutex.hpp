#pragma once

#include "decl.hxx"
#include "err.hpp"
#include "MutexGuard.hpp"

namespace cornus {

class CondMutex {
public:
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	mutable pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	bool exit = false;
	
	inline bool Broadcast() {
		int status = pthread_cond_broadcast(&cond);
		const bool ok = (status == 0);
		if (!ok)
			mtl_status(status);
		return ok;
	}
	
	inline int CondWait() {
		return pthread_cond_wait(&cond, &mutex);
	}
	
	MutexGuard guard(const Lock l = Lock::Yes) const {
		return (l == Lock::Yes) ? MutexGuard(&mutex) : MutexGuard();
	}
	
	inline bool Lock() {
		int status = pthread_mutex_lock(&mutex);
		const bool ok = (status == 0);
		if (!ok)
			mtl_status(status);
		return ok;
	}
	
	inline void Signal() {
		int status = pthread_cond_signal(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	inline bool Unlock() {
		return (pthread_mutex_unlock(&mutex) == 0);
	}
};
} // cornus::
