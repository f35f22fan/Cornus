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
#include "io/decl.hxx"
#include "io/Server.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "gui/TasksWin.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);

namespace cornus {
struct args_data {
	io::Server *server = nullptr;
	int fd = -1;
};

void* ProcessRequest(void *ptr)
{
	pthread_detach(pthread_self());
	cornus::ByteArray ba;
	args_data *args = (args_data*)ptr;
	int fd = args->fd;
	cornus::io::Server *server = args->server;
	gui::TasksWin *tasks_win = server->tasks_win();
	delete args;
	args = nullptr;
	
	if (!ba.Receive(fd, false)) {
		mtl_warn("receive failed");
		return nullptr;
	}
	
	const auto msg = ba.next_u32() & ~(7u << 29);
	const auto ConnectionType = Qt::BlockingQueuedConnection;
	using io::socket::MsgType;
	
	switch (msg) {
	case (MsgType)io::socket::MsgBits::CheckAlive: {
		close(fd);
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::CopyToClipboard: {
		close(fd);
		QString s = ba.next_string();
		QMetaObject::invokeMethod(server, "CopyToClipboard",
			ConnectionType, Q_ARG(QString, s));
		
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::CutToClipboard: {
		close(fd);
		QString s = ba.next_string();
		QMetaObject::invokeMethod(server, "CutToClipboard",
			ConnectionType, Q_ARG(QString, s));
		
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::SendOpenWithList: {
		QString mime = ba.next_string();
		QMetaObject::invokeMethod(server, "SendOpenWithList",
			ConnectionType, Q_ARG(QString, mime), Q_ARG(int, fd));
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::SendDesktopFilesById: {
		QMetaObject::invokeMethod(server, "SendDesktopFilesById",
			ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::SendAllDesktopFiles: {
		QMetaObject::invokeMethod(server, "SendAllDesktopFiles",
			ConnectionType, Q_ARG(int, fd));
		return nullptr;
	}
	} /// switch()
	
	close(fd);
	ba.to(0);
	auto *task = cornus::io::Task::From(ba);
	
	if (task != nullptr) {
		QMetaObject::invokeMethod(tasks_win, "add",
			Q_ARG(cornus::io::Task*, task));
	}
	
	task->StartIO();
	
	return nullptr;
}


void* StartServerSocket(void *args)
{
	pthread_detach(pthread_self());
	
	auto *server = (io::Server*)args;
	
	int server_sock_fd = cornus::io::socket::Server(cornus::SocketPath);
	if (server_sock_fd == -1) {
		mtl_info("Another cornus_io is running. Exiting.");
		exit(0);
	}
	
	pthread_t th;
	
	while (true) {
		int client_fd = accept(server_sock_fd, NULL, NULL);
		
		if (client_fd == -1) {
			mtl_status(errno);
			break;
		}
		
		auto *args = new args_data();
		args->fd = client_fd;
		args->server = server;
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
	pid_t pid = getpid();
	printf("cornus_io PID: %ld\n", i64(pid));
	
	QApplication qapp(argc, argv);
	qRegisterMetaType<cornus::io::Task*>();
	qRegisterMetaType<cornus::ByteArray*>();
	
	/// The TasksWin hides/deletes itself
	/// Currently it deletes itself, change later to just hide itself.
	auto *server = new cornus::io::Server();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::StartServerSocket, server);
	if (status != 0)
		mtl_status(status);
	
	return qapp.exec();
}

