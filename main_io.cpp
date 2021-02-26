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
const auto ConnectionType = Qt::BlockingQueuedConnection;
struct args_data {
	io::Server *server = nullptr;
	int fd = -1;
};

void ExtractArchives(ByteArray *ba, io::Server *server)
{
	QString to_dir = ba->next_string();
	mtl_printq2("to dir: ", to_dir);
	QVector<QString> urls;
	while (ba->has_more()) {
		QString s = ba->next_string();
		urls.append(s);
		mtl_printq2("Extract file: ", s);
	}
	
	if (urls.isEmpty()) {
		mtl_info("Returning");
		return;
	}
	
	QProcess *ps = new QProcess();
	ps->setWorkingDirectory(to_dir);
	ps->setProgram(QLatin1String("ark"));
	QStringList args;
	
	args.append(QLatin1String("-b"));
	args.append(QLatin1String("-a"));
	
	for (const auto &next: urls) {
		args.append(next);
	}
	
	ps->setArguments(args);
	
	cornus::io::ArchiveInfo info;
	ps->start();
	info.urls = urls;
	info.to_dir = to_dir;
	
	CHECK_TRUE_VOID(ps->waitForStarted());
	info.pid = ps->processId();
	
	QObject::connect(ps, &QProcess::errorOccurred, [server, &info] {
		mtl_info("An error occured!");
		QMetaObject::invokeMethod(server, "ExtractingArchiveFinished",
			ConnectionType, Q_ARG(const i64, info.pid));
	});

	
	QMetaObject::invokeMethod(server, "ExtractingArchiveStarted",
		ConnectionType, Q_ARG(cornus::io::ArchiveInfo*, &info));
	
	const i64 max_wait_time = 120 * 60 * 1000; // 2 hours
	CHECK_TRUE_VOID(ps->waitForFinished(max_wait_time));
	
	QMetaObject::invokeMethod(server, "ExtractingArchiveFinished",
		ConnectionType, Q_ARG(const i64, info.pid));
}

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
	case (MsgType)io::socket::MsgBits::ExtractArchives: {
		close(fd);
		ExtractArchives(&ba, server);
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
	qRegisterMetaType<cornus::io::ArchiveInfo*>();
	
	/// The TasksWin hides/deletes itself
	/// Currently it deletes itself, change later to just hide itself.
	auto *server = new cornus::io::Server();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::StartServerSocket, server);
	if (status != 0)
		mtl_status(status);
	
	return qapp.exec();
}

