#pragma once

#include "decl.hxx"
#include "err.hpp"
#include "MutexGuard.hpp"

namespace cornus {

class CondMutex {
public:
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	mutable pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	union Data {
		char *ptr;
		bool flag;
	} data;
	
	inline void Broadcast() {
		pthread_cond_broadcast(&cond);
	}
	
	inline int CondWait() {
		return pthread_cond_wait(&cond, &mutex);
	}
	
	MutexGuard guard(const Lock l = Lock::Yes) const {
		return (l == Lock::Yes) ? MutexGuard(&mutex) : MutexGuard();
	}
	
	inline bool Lock(const enum Lock l = Lock::Yes)
	{
		if (l != Lock::Yes)
			return true;
		cint status = pthread_mutex_lock(&mutex);
		const bool ok = (status == 0);
		if (!ok)
			mtl_status(status);
		return ok;
	}
	
	inline void Signal() {
		cint status = pthread_cond_signal(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	inline bool Unlock(const enum Lock l = Lock::Yes)
	{
		if (l != Lock::Yes)
			return true;
		return (pthread_mutex_unlock(&mutex) == 0);
	}
	
	void SetFlag(const bool b, const enum Lock l = Lock::Yes) {
		Lock(l);
		data.flag = b;
		Unlock(l);
	}
	
	bool GetFlag(const enum Lock l = Lock::Yes) {
		Lock(l);
		const bool b = data.flag;
		Unlock(l);
		return b;
	}
	
	void SetPtr(char *p, const enum Lock l = Lock::Yes) {
		Lock(l);
		data.ptr = p;
		Unlock(l);
	}
	
	char* GetPtr(const enum Lock l = Lock::Yes)
	{
		Lock(l);
		char *p = data.ptr;
		Unlock(l);
		return p;
	}
};

class Mutex {
public:
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	union Data {
		char *ptr;
		bool flag;
	} data;
	
	MutexGuard guard(const enum Lock l = Lock::Yes) const {
		return (l == Lock::Yes) ? MutexGuard(&mutex) : MutexGuard();
	}
	
	inline bool TryLock() {
		cint status = pthread_mutex_trylock(&mutex);
		if (status == 0)
			return true;
		mtl_status(status);
		return false;
	}
	
	inline bool Lock(const enum Lock l = Lock::Yes) {
		if (l != Lock::Yes)
			return true;
		cint status = pthread_mutex_lock(&mutex);
		const bool ok = (status == 0);
		if (!ok)
			mtl_status(status);
		return ok;
	}
	
	inline bool Unlock(const enum Lock l = Lock::Yes) {
		if (l != Lock::Yes)
			return true;
		return (pthread_mutex_unlock(&mutex) == 0);
	}
	
	void SetFlag(const bool b) {
		Lock();
		data.flag = b;
		Unlock();
	}
	
	bool GetFlag(const enum Lock l = Lock::Yes) {
		Lock(l);
		const bool b = data.flag;
		Unlock(l);
		return b;
	}
	
	void SetPtr(char *p) {
		Lock();
		data.ptr = p;
		Unlock();
	}
	
	char* GetPtr() {
		Lock();
		char *p = data.ptr;
		Unlock();
		return p;
	}
};

} // cornus::
