#pragma once

namespace cornus {

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
