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
	
	void Continue(const Reset r = Reset::No);
	void Pause();
	
	i64 elapsed_nano();
	i64 elapsed_mc() { return elapsed_nano() / 1000L; }
	i64 elapsed_ms() { return elapsed_nano() / 1000000L; }
	
private:
	NO_ASSIGN_COPY_MOVE(ElapsedTimer);
	struct timespec start_ = {};
	i64 worked_time_nano_ = 0;
	
	/// CLOCK_MONOTONIC_COARSE is faster but less precise
	/// CLOCK_MONOTONIC_RAW is slower but more precise
	const clockid_t clock_type_ = CLOCK_MONOTONIC_RAW;
};
}
