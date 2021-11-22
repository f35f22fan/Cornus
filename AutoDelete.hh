#pragma once

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
