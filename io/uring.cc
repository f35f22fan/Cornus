#include "uring.hh"

#include "../AutoDelete.hh"

#include <sys/inotify.h>
#include <linux/types.h> // struct __kernel_timespec
#include <pthread.h>

#include <QDir>

namespace cornus::uring {

void ProcessEvent(struct io_uring_cqe *cqe)
{
	struct UserData *data = (UserData*)io_uring_cqe_get_data(cqe);
	char *buf = (char*) data->iv.iov_base;
	const ssize_t num_read = cqe->res;
	i64 add = 0;
	
	for (char *p = buf; p < buf + num_read; p += add)
	{
		struct inotify_event *ev = (struct inotify_event*) p;
		add = sizeof(struct inotify_event) + ev->len;
		const auto mask = ev->mask;
		const bool is_dir = mask & IN_ISDIR;
		mtl_info("is_dir: %s, name: \"%s\"", (is_dir ? "true" : "false"), ev->name);
		bool processed = false;
		
		if (mask & IN_ATTRIB) {
			processed = true;
			mtl_info("IN_ATTRIB: %s\n", ev->name);
		}
		if (mask & IN_CREATE) {
			processed = true;
			mtl_info("IN_CREATE: %s\n", ev->name);
		}
		if (mask & IN_DELETE_SELF) {
			processed = true;
			mtl_info("IN_DELETE_SELF: %s\n", ev->name);
			break;
		}
		if (mask & IN_MOVE_SELF) {
			processed = true;
			mtl_info("IN_MOVE_SELF: %s\n", ev->name);
		}
		if (mask & IN_MOVED_FROM) {
			processed = true;
			mtl_info("IN_MOVED_FROM: %s\n", ev->name);
		}
		if (mask & IN_MOVED_TO) {
			processed = true;
			mtl_info("IN_MOVED_TO: %s\n", ev->name);
		}
		if (mask & IN_Q_OVERFLOW) {
			processed = true;
			mtl_info("IN_Q_OVERFLOW: %s\n", ev->name);
		}
		if (mask & IN_UNMOUNT) {
			processed = true;
			mtl_info("IN_UNMOUNT: %s\n", ev->name);
			break;
		}
		if (mask & IN_CLOSE_WRITE) {
			processed = true;
			mtl_info("IN_CLOSE_WRITE: %s\n", ev->name);
		}
		if (mask & IN_IGNORED) {
			processed = true;
			mtl_info("IN_IGNORED: %s\n", ev->name);
		}
		if (mask & IN_CLOSE_NOWRITE) {
			processed = true;
			mtl_info("IN_CLOSE_NOWRITE: %s\n", ev->name);
		}
		if (mask & IN_OPEN) {
			processed = true;
			mtl_info("IN_OPEN: %s\n", ev->name);
		}
		
		if (!processed){
			mtl_warn("Unhandled inotify event: %u", mask);
		}
	}
}

void monitor_file(struct io_uring *ring)
{
	while(true)
	{
		struct io_uring_cqe *cqe = nullptr;
		int ret = io_uring_wait_cqe(ring, &cqe);
		if (ret < 0) {
			mtl_errno();
			break;
		}
		if (cqe->res < 0) {
			io_uring_cqe_seen(ring, cqe);
			mtl_warn("Async readv failed: %s", strerror(-cqe->res));
			break;
		}
		
		ProcessEvent(cqe);
		struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
		io_uring_sqe_set_data(sqe, io_uring_cqe_get_data(cqe));
		io_uring_cqe_seen(ring, cqe);
		io_uring_submit(ring);
	}
}

bool submit_inotify(const int watch_fd, struct io_uring *ring)
{
	const i64 buf_size = 4096;
	
	UserData *data = new UserData();
	data->iv.iov_base = malloc(buf_size);
	memset(data->iv.iov_base, 0, buf_size);
	data->iv.iov_len = buf_size;
	data->inotify_fd = watch_fd;
	
	int ret = io_uring_register_buffers(ring, &data->iv, 1);
	if (ret) {
		mtl_status(-ret);
		return false;
	}
	
	{
		struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
		io_uring_prep_read_fixed(sqe, watch_fd, data->iv.iov_base, data->iv.iov_len, 0, 0);
		io_uring_sqe_set_data(sqe, data); // Set user data
	}
	
	io_uring_submit(ring);
	
	return true;
}

void* DoTest(void *params)
{
	pthread_detach(pthread_self());
	
	auto home = QDir::home().filePath("Documents").toLocal8Bit();
	const char *path = home.data();
	auto event_types = IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF
		| IN_MOVE_SELF | IN_CLOSE_WRITE | IN_MOVE;
	int inotify_fd = inotify_init();
	RET_IF(inotify_fd, -1, NULL);
	AutoCloseFd notify_autoclose(inotify_fd);
	int watch_fd = inotify_add_watch(inotify_fd, path, event_types);
	
	if (watch_fd == -1) {
		mtl_status(errno);
		return NULL;
	}
	
	struct io_uring ring;
	io_uring_queue_init(kQueueDepth, &ring, 0);
	RET_IF(submit_inotify(inotify_fd, &ring), false, nullptr);
	monitor_file(&ring);
	io_uring_queue_exit(&ring); // clean-up function
	if (watch_fd != -1) {
		inotify_rm_watch(inotify_fd, watch_fd);
	}
	
	return NULL;
}

} // namespace
