#include "ElapsedTimer.hpp"

namespace cornus {

ElapsedTimer::ElapsedTimer() {}
ElapsedTimer::~ElapsedTimer() {}

inline i8 GetDiffMs(const timespec &end, const timespec &start) {
	return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_nsec - start.tv_nsec) / 1000000L;
}

inline i8 GetDiffMc(const timespec &end, const timespec &start) {
	return (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_nsec - start.tv_nsec) / 1000L;
}

inline i8 GetDiffNano(const timespec &end, const timespec &start) {
	return (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
}

void ElapsedTimer::Continue(const cornus::Reset r)
{
	if (r == Reset::Yes)
		worked_time_nano_ = 0;
	
	const int status = clock_gettime(clock_type_, &start_);
	if (status != 0)
		mtl_status(errno);
}

i8 ElapsedTimer::elapsed_nano()
{
	const bool paused = (start_.tv_sec == 0 && start_.tv_nsec == 0);
	if (paused)
		return worked_time_nano_;
	
	struct timespec now;
	int status = clock_gettime(clock_type_, &now);
	if (status != 0)
		mtl_status(errno);
	
	const i8 extra = GetDiffNano(now, start_);
	return worked_time_nano_ + extra;
}

void ElapsedTimer::Pause()
{
	struct timespec now;
	const int status = clock_gettime(clock_type_, &now);
	if (status != 0)
		mtl_status(errno);
	
	worked_time_nano_ += GetDiffNano(now, start_);
	start_ = {};
}

}
