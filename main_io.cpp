#include <cstdlib>

#include <QApplication>
#include <QDir>
#include <QTextStream>
#include <QWidget>

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "AutoDelete.hh"
#include "ByteArray.hpp"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/Server.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "gui/TasksWin.hpp"
#include "MyDaemon.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);

///#define CORNUS_DEBUG_SERVER_SHUTDOWN

namespace cornus {
const auto ConnectionType = Qt::BlockingQueuedConnection;
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
	
	CHECK_TRUE_NULL(ba.Receive(fd, CloseSocket::No));
	const auto msg = ba.next_u32() & ~(7u << 29);
	using io::socket::MsgType;
	
	switch (msg) {
	case (MsgType)io::socket::MsgBits::CheckAlive: {
		close(fd);
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::CopyToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(server, "CopyURLsToClipboard",
			ConnectionType, Q_ARG(ByteArray*, &ba));
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::CutToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(server, "CutURLsToClipboard",
			ConnectionType, Q_ARG(ByteArray*, &ba));
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::SendOpenWithList: {
		QString mime = ba.next_string();
		QMetaObject::invokeMethod(server, "SendOpenWithList",
			ConnectionType, Q_ARG(QString, mime), Q_ARG(int, fd));
		return nullptr;
	}
	case (MsgType)io::socket::MsgBits::SendDefaultDesktopFileForFullPath: {
		QMetaObject::invokeMethod(server, "SendDefaultDesktopFileForFullPath",
			ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
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
	case (MsgType) io::socket::MsgBits::QuitServer: {
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
		mtl_info("Received QuitServer signal over socket");
#endif
		close(fd);
		
		cornus::io::ServerLife &life = server->life();
		life.Lock();
		life.exit = true;
		life.Unlock();
		
		ByteArray ba;
		ba.set_msg_id(io::socket::MsgBits::None);
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
		mtl_info("Waking up server to process it...");
#endif
		io::socket::SendSync(ba);
		return nullptr;
	}
	} /// switch()
	
	close(fd);
	ba.to(0);
	auto *task = cornus::io::Task::From(ba);
	if (task != nullptr)
	{
		QMetaObject::invokeMethod(tasks_win, "add",
			ConnectionType, Q_ARG(cornus::io::Task*, task));
		task->StartIO();
	}
	
	return nullptr;
}

void* ListenTh(void *args)
{
	pthread_detach(pthread_self());
	auto *server = (io::Server*)args;
	io::ServerLife &life = server->life();
	
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
		
		if (life.Lock())
		{
			const bool do_exit = life.exit;
			life.Unlock();
			
			if (do_exit) {
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
				mtl_info("Server is quitting");
#endif
				break;
			}
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
	
	close(server_sock_fd);
	QMetaObject::invokeMethod(server, "QuitGuiApp", ConnectionType);
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
	mtl_info("Server socket closed");
#endif
	
	return nullptr;
}
}

static int setup_unix_signal_handlers()
{
	struct sigaction hup, term;
	
	hup.sa_handler = cornus::MyDaemon::hupSignalHandler;
	sigemptyset(&hup.sa_mask);
	hup.sa_flags = 0;
	hup.sa_flags |= SA_RESTART;
	
	if (sigaction(SIGINT, &hup, 0))
		return 1;
	
	term.sa_handler = cornus::MyDaemon::termSignalHandler;
	sigemptyset(&term.sa_mask);
	term.sa_flags = 0;
	term.sa_flags |= SA_RESTART;
	
	if (sigaction(SIGTERM, &term, 0))
		return 2;
	
	return 0;
}

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	printf("%s PID: %ld\n", argv[0], i64(pid));
	QApplication qapp(argc, argv);
	qRegisterMetaType<cornus::io::Task*>();
	qRegisterMetaType<cornus::ByteArray*>();
	
	setup_unix_signal_handlers();
	new cornus::MyDaemon();
	
	/// The TasksWin hides/deletes itself
	/// Currently it deletes itself, change later to just hide itself.
	cornus::io::Server *server = new cornus::io::Server();
	cornus::AutoDelete server___(server);
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::ListenTh, server);
	if (status != 0)
		mtl_status(status);
	
	cornus::io::ServerLife &life = server->life();
	int ret;
	while (true)
	{
		ret = qapp.exec();
		
		life.Lock();
		const bool do_exit = life.exit;
		life.Unlock();
		if (do_exit)
			break;
	}
	
	return ret;
}

