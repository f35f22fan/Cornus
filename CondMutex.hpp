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
		struct {
			bool act = false;
			bool exit = false;
		};
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
		return (status == 0);
	}
	
	inline bool TryLock() {
		cint status = pthread_mutex_trylock(&mutex);
		return (status == 0);
	}
	
	inline void Signal() {
		pthread_cond_signal(&cond);
	}
	
	inline bool Unlock(const enum Lock l = Lock::Yes)
	{
		return (l != Lock::Yes) ? true : (pthread_mutex_unlock(&mutex) == 0);
	}
	
	void SetFlag(cbool b, const enum Lock l = Lock::Yes) {
		Lock(l);
		data.act = b;
		Unlock(l);
	}
	
	bool GetFlag(const enum Lock l = Lock::Yes) {
		Lock(l);
		cbool b = data.act;
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
		bool act;
	} data;
	
	MutexGuard guard(const enum Lock l = Lock::Yes) const {
		return (l == Lock::Yes) ? MutexGuard(&mutex) : MutexGuard();
	}
	
	inline bool TryLock() {
		return (pthread_mutex_trylock(&mutex) == 0);
	}
	
	inline bool Lock(const enum Lock l = Lock::Yes) {
		if (l != Lock::Yes)
			return true;
		cint status = pthread_mutex_lock(&mutex);
		return (status == 0);
	}
	
	inline bool Unlock(const enum Lock l = Lock::Yes) {
		if (l != Lock::Yes)
			return true;
		return (pthread_mutex_unlock(&mutex) == 0);
	}
	
	void SetFlag(cbool b) {
		Lock();
		data.act = b;
		Unlock();
	}
	
	bool GetFlag(const enum Lock l = Lock::Yes) {
		Lock(l);
		cbool b = data.act;
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
