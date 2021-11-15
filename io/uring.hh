#pragma once

#include "../err.hpp"

#include <fcntl.h>
#include <cstdio>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <liburing.h>
#include <cstdlib>

namespace cornus::uring {

const int kQueueDepth = 1;

struct UserData {
	//struct iovec iovecs[]; // Referred by readv/writev
	struct iovec iv;
	int inotify_fd = -1;
};

void get_completion_and_print(struct io_uring *ring);
void *DoTest(void *params);
}
