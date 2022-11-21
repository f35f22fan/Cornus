#pragma once

// pkg_search_module(URING REQUIRED liburing)
#include "err.hpp"

#include <fcntl.h>
#include <cstdio>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <liburing.h>
#include <cstdlib>

#include <QVector>

namespace cornus::uring {

struct BufArg {
	int fd;
	i32 buf_size;
};

struct UserData {
	//struct iovec iovecs[]; // Referred by readv/writev
	struct iovec iv;
	int fd = -1;
};

bool SubmitBuffers(struct io_uring *ring, const QVector<BufArg> &vec);

void get_completion_and_print(struct io_uring *ring);
void *DoTest(void *params);
}
