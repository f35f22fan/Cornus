#pragma once

#include "err.hpp"
#include "MutexGuard.hpp"

#include <pthread.h>

namespace cornus {

struct GuiBits {
	static const u16 bit_created = 1u << 0;
	u16 bits_ = 0;
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	
	GuiBits() {}
	~GuiBits() {}
	GuiBits(const GuiBits &rhs) = delete;
	
	inline void Broadcast() {
		int status = pthread_cond_broadcast(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	inline int CondWait() {
		return pthread_cond_wait(&cond, &mutex);
	}
	
	inline bool created() const { return bits_ & bit_created; }
	inline void created(const bool flag) {
		if (flag)
			bits_ |= bit_created;
		else
			bits_ &= ~bit_created;
	}
	
	MutexGuard guard() const {
		return MutexGuard(&mutex);
	}
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status;
	}
	
	inline void Signal() {
		int status = pthread_cond_signal(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	inline int Unlock() {
		return pthread_mutex_unlock(&mutex);
	}
};


}
