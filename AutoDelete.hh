#pragma once

#include <sys/inotify.h>
#include <libudev.h>

#include "gui/decl.hxx"
#include "MutexGuard.hpp"
#include "io/io.hh"

namespace cornus {

class UdevAutoUnref {
public:
	UdevAutoUnref(struct udev *p): p_(p) {}
	~UdevAutoUnref() {
		if (p_)
			udev_unref(p_);
	}
private:
	struct udev *p_ = nullptr;
};

class UdevDeviceAutoUnref {
public:
	UdevDeviceAutoUnref(struct udev_device *p): p_(p) {}
	~UdevDeviceAutoUnref() {
		if (p_)
			udev_device_unref(p_);
	}
private:
	struct udev_device *p_ = nullptr;
};

class UdevMonitorAutoUnref {
public:
	UdevMonitorAutoUnref(struct udev_monitor *p): p_(p) {}
	~UdevMonitorAutoUnref() {
		if (p_)
			udev_monitor_unref(p_);
	}
private:
	struct udev_monitor *p_ = nullptr;
};

class AutoCloseFd {
public:
	AutoCloseFd(int fd): fd_(fd) {}
	~AutoCloseFd() {
		if (fd_ != -1)
			::close(fd_);
	}
private:
	int fd_ = -1;
};

class AutoRemoveWatch {
public:
	AutoRemoveWatch(io::Notify &notify, int wd):
		notify_(notify), wd_(wd)
	{
		MutexGuard guard(&notify.watches_mutex);
		int count = notify.watches.value(wd_, 0);
		notify.watches[wd_] = count + 1;
	}
	
	void RemoveWatch(int wd) {
		notify_.watches.remove(wd); /// needed on IN_UNMOUNT event
	}
	
	~AutoRemoveWatch() {
		bool contains_wd = false;
		{
			MutexGuard guard(&notify_.watches_mutex);
			contains_wd = notify_.watches.contains(wd_);
			int count = notify_.watches.value(wd_) - 1;
			
			if (count > 0) {
				notify_.watches[wd_] = count;
				return;
			}
			
			notify_.watches.remove(wd_);
		}
		
		if (contains_wd) {
			int status = inotify_rm_watch(notify_.fd, wd_);
			if (status != 0)
				mtl_warn("%s: %d", strerror(errno), wd_);
		}
	}
private:
	io::Notify &notify_;
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
