#pragma once

#include <sys/inotify.h>

namespace cornus {
    
class AutoRemoveWatch {
public:
    AutoRemoveWatch(int inotify_fd, int wd): fd_(inotify_fd), wd_(wd) {}
    ~AutoRemoveWatch() {
        int status = inotify_rm_watch(fd_, wd_);
        if (status != 0)
            perror("inotify_rm_watch");
    }
private:
    int fd_;
    int wd_;
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
