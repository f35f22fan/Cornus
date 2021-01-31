#pragma once

#include <time.h>
#include <cstdio>

#include "err.hpp"

namespace cornus {

class ElapsedTimer {
public:
	ElapsedTimer();
	~ElapsedTimer();
	
	void Continue();
	void Pause();
	i64 elapsed_ms();
	
private:
	NO_ASSIGN_COPY_MOVE(ElapsedTimer);
	struct timespec start_ = {};
	i64 worked_time_ = 0;
	
	/// CLOCK_MONOTONIC_COARSE is faster but less precise
	/// CLOCK_MONOTONIC_RAW is slower but more precise
	const clockid_t clock_type_ = CLOCK_MONOTONIC_RAW;
};
}
