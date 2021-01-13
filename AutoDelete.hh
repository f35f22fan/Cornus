#pragma once

#include <sys/inotify.h>
#include "gui/decl.hxx"
#include "MutexGuard.hpp"

namespace cornus {

class AutoRemoveWatch {
public:
	AutoRemoveWatch(cornus::gui::Notify &notify, int wd):
		notify_(notify), wd_(wd)
	{
		MutexGuard guard(&notify.watches_mutex);
		int count = notify.watches.value(wd_, 0);
		notify.watches[wd_] = count + 1;
	}
	~AutoRemoveWatch() {
		{
			MutexGuard guard(&notify_.watches_mutex);
			int count = notify_.watches.value(wd_) - 1;
			
			if (count > 0) {
				//mtl_info("Skipped removing %d, new count: %d", wd_, count);
				notify_.watches[wd_] = count;
				return;
			}
			
			notify_.watches.remove(wd_);
		}
		int status = inotify_rm_watch(notify_.fd, wd_);
		if (status != 0)
			mtl_warn("%s: %d", strerror(errno), wd_);
	}
private:
	cornus::gui::Notify &notify_;
	int wd_ = -1;
};

template <class A_Type> class AutoFree {
public:
	AutoFree(A_Type x) : x_(x) {}
	virtual ~AutoFree() { free(x_); }
	
private:
	A_Type x_ = nullptr;
};

template <class A_Type> class AutoDelete {
public:
	AutoDelete(A_Type x) : x_(x) {}
	virtual ~AutoDelete() { delete x_; }
	
private:
	A_Type x_ = nullptr;
};

template <class A_Type> class AutoDeleteArr {
public:
	AutoDeleteArr(A_Type x) : x_(x) {}
	virtual ~AutoDeleteArr() { delete [] x_; x_ = nullptr; }
	
private:
	A_Type x_ = nullptr;
};

template <class VecType> class AutoDeleteVec {
public:
	AutoDeleteVec(VecType &x) : vec_(x) {}
	~AutoDeleteVec() {
		for (auto *item : vec_) {
			delete item;
		}
	}
	
private:
	VecType &vec_;
};

template <class VecType> class AutoDeleteVecP {
public:
	AutoDeleteVecP(VecType &x) : vec_(x) {}
	~AutoDeleteVecP() {
		for (auto *item : *vec_) {
			delete item;
		}
		delete vec_;
	}
	
private:
	VecType vec_;
};
}
