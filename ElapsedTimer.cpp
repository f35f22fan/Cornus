#include "ElapsedTimer.hpp"

namespace cornus {

ElapsedTimer::ElapsedTimer() {}
ElapsedTimer::~ElapsedTimer() {}

inline i64 GetDiffMs(const timespec &end, const timespec &start) {
	return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_nsec - start.tv_nsec) / 1000000L;
}

void ElapsedTimer::Continue()
{
	int status = clock_gettime(clock_type_, &start_);
	if (status != 0)
		printf("%s", strerror(errno));
}

i64 ElapsedTimer::elapsed_ms()
{
	const bool paused = (start_.tv_sec == 0 && start_.tv_nsec == 0);
	if (paused)
		return worked_time_;
	
	struct timespec now;
	int status = clock_gettime(clock_type_, &now);
	if (status != 0)
		printf("%s", strerror(errno));
	
	const i64 extra = GetDiffMs(now, start_);
	return worked_time_ + extra;
}

void ElapsedTimer::Pause() {
	struct timespec now;
	int status = clock_gettime(clock_type_, &now);
	if (status != 0)
		printf("%s", strerror(errno));
	
	worked_time_ += GetDiffMs(now, start_);
	start_ = {};
}

}
