#include <cstdlib>

#include <QApplication>
#include <QWidget>

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "AutoDelete.hh"
#include "ByteArray.hpp"
#include "decl.hxx"
#include "err.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "gui/TasksWin.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);

namespace cornus {
struct args_data {
	cornus::gui::TasksWin *tasks_win = nullptr;
	int fd = -1;
};

void* ProcessRequest(void *ptr)
{
	pthread_detach(pthread_self());
	cornus::ByteArray ba;
	args_data *args = (args_data*)ptr;
	int fd = args->fd;
	gui::TasksWin *tasks_win = args->tasks_win;
	delete args;
	args = nullptr;
	
	if (!ba.Receive(fd)) {
		mtl_warn("receive failed");
		return nullptr;
	}
	
	const auto msg = ba.next_u32();
	
	if (msg == io::socket::MsgBits::CheckAlive) {
		mtl_info("Someone checked if alive");
		return nullptr;
	}
	
	ba.to(0);
	auto *task = cornus::io::Task::From(ba);
	
	if (task != nullptr) {
		QMetaObject::invokeMethod(tasks_win, "add",
			Q_ARG(cornus::io::Task*, task));
	}
	
	//task->WaitForStartSignal();
	task->StartIO();
	
	return nullptr;
}


void* StartServerSocket(void *args)
{
	pthread_detach(pthread_self());
	
	auto *tasks_win = (gui::TasksWin*)args;
	
	int server_sock_fd = cornus::io::socket::Server(cornus::SocketPath);
	CHECK_TRUE_NULL((server_sock_fd != -1));
	pthread_t th;
	
	while (true) {
		int client_fd = accept(server_sock_fd, NULL, NULL);
		
		if (client_fd == -1) {
			mtl_status(errno);
			break;
		}
		
		auto *args = new args_data();
		args->fd = client_fd;
		args->tasks_win = tasks_win;
		int status = pthread_create(&th, NULL, ProcessRequest, args);
		if (status != 0) {
			mtl_status(status);
			break;
		}
	}
	
	mtl_info("Server socket closed");
	return nullptr;
}
}

int main(int argc, char *argv[])
{
	QApplication qapp(argc, argv);
	qRegisterMetaType<cornus::io::Task*>();
	
	/// The TasksWin hides/deletes itself
	/// Currently it deletes itself, change later to just hide itself.
	auto *tasks_win = new cornus::gui::TasksWin();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::StartServerSocket, tasks_win);
	if (status != 0)
		mtl_status(status);
	
	return qapp.exec();
}

