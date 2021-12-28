#pragma once

#include <time.h>
#include <cstdio>

#include "err.hpp"

namespace cornus {

enum class Reset: i8 {
	Yes,
	No
};

class ElapsedTimer {
public:
	ElapsedTimer();
	~ElapsedTimer();
	
	void Continue(const cornus::Reset r = Reset::No);
	void Pause();
	
	i64 elapsed_mc();
	i64 elapsed_ms() { return elapsed_mc() / 1000L; }
	
private:
	NO_ASSIGN_COPY_MOVE(ElapsedTimer);
	struct timespec start_ = {};
	i64 worked_time_ = 0;
	
	/// CLOCK_MONOTONIC_COARSE is faster but less precise
	/// CLOCK_MONOTONIC_RAW is slower but more precise
	const clockid_t clock_type_ = CLOCK_MONOTONIC_RAW;
};
}
