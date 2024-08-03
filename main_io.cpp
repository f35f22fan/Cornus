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

#include <QMetaMethod>

#include "AutoDelete.hh"
#include "ByteArray.hpp"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "io/Daemon.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "trash.hh"
#include "gui/TasksWin.hpp"
#include "MyDaemon.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);
Q_DECLARE_METATYPE(cornus::io::Daemon*);

///#define CORNUS_DEBUG_SERVER_SHUTDOWN

namespace cornus {
cauto ConnectionType = Qt::BlockingQueuedConnection;
void* ProcessRequestTh(void *ptr);
struct Args
{
	io::Daemon *daemon = nullptr;
	int fd = -1;
};

void* ListenTh(void *ptr)
{
	pthread_detach(pthread_self());
	
	auto *daemon = (io::Daemon*)ptr;
	io::ServerLife *life = daemon->life();
	
	int daemon_sock_fd = io::socket::Daemon(cornus::SocketPath, PrintErrors::No);
	if (daemon_sock_fd == -1)
	{
		mtl_info("Another cornus_io is running. Exiting.");
		QMetaObject::invokeMethod(daemon, "QuitGuiApp", ConnectionType);
		return nullptr;
	}
	
	while (true)
	{
		cint client_fd = accept(daemon_sock_fd, NULL, NULL);
		if (client_fd == -1)
		{
			mtl_status(errno);
			break;
		}
		
		if (life->Lock())
		{
			const bool do_exit = life->exit;
			life->Unlock();
			if (do_exit)
				break;
		}
		
		auto *args = new Args();
		args->fd = client_fd;
		args->daemon = daemon;
		if (!io::NewThread(ProcessRequestTh, args))
			break;
	}
	
	close(daemon_sock_fd);
	QMetaObject::invokeMethod(daemon, "QuitGuiApp", ConnectionType);
	
	return nullptr;
}

void* ProcessRequestTh(void *ptr)
{
	pthread_detach(pthread_self());
	cornus::ByteArray ba;
	auto *args = (Args*)ptr;
	cint fd = args->fd;
	cornus::io::Daemon *daemon = args->daemon;
	delete args;
	args = nullptr;
	
	gui::TasksWin *tasks_win = daemon->tasks_win();
	MTL_CHECK_ARG(ba.Receive(fd, CloseSocket::No), nullptr);
	cauto msg_int = ba.next_u32() & ~(io::MessageBitsMask << io::MessageBitsStartAt);
	cauto msg = static_cast<io::Message>(msg_int);
	switch (msg)
	{
	case io::Message::CheckAlive: {
		close(fd);
		return nullptr;
	}
	case io::Message::CopyToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(daemon, "CopyURLsToClipboard",
		ConnectionType, Q_ARG(ByteArray*, &ba));
		return nullptr;
	}
	case io::Message::CutToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(daemon, "CutURLsToClipboard",
		ConnectionType, Q_ARG(ByteArray*, &ba));
		return nullptr;
	}
	case io::Message::EmptyTrashRecursively: {
		close(fd);
		QString dir_path = ba.next_string();
		QMetaObject::invokeMethod(daemon, "EmptyTrashRecursively",
		ConnectionType, Q_ARG(QString, dir_path), Q_ARG(bool, true));
		return nullptr;
	}
	case io::Message::SendOpenWithList: {
		QString mime = ba.next_string();
		QMetaObject::invokeMethod(daemon, "SendOpenWithList",
		ConnectionType, Q_ARG(QString, mime), Q_ARG(cint, fd));
		return nullptr;
	}
	case io::Message::SendDefaultDesktopFileForFullPath: {
		// const QMetaObject *meta = daemon->metaObject();
		// cint index = meta->indexOfMethod("Send1");
		// mtl_check_arg(index != -1, nullptr);
		// QMetaMethod method = meta->method(index);
		// method.invoke(0, Qt::DirectConnection,
		// 	&ba, (int)fd);
		
		// cint count = meta->methodCount();
		// for (int i = 0; i < count; i++) {
		// 	QMetaMethod method = meta->method(i);
		// 	auto name = method.name();
		// 	mtl_info("method %d: %s\n", i, name.data());
		// }
		
		QMetaObject::invokeMethod(daemon, "SendDefaultDesktopFileForFullPath", ConnectionType,
			Q_ARG(ByteArray*, &ba), Q_ARG(cint, fd));
		return nullptr;
	}
	case io::Message::SendDesktopFilesById: {
		QMetaObject::invokeMethod(daemon, "SendDesktopFilesById",
		ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(cint, fd));
		return nullptr;
	}
	case io::Message::SendAllDesktopFiles: {
		QMetaObject::invokeMethod(daemon, "SendAllDesktopFiles",
		ConnectionType, Q_ARG(cint, fd));
		return nullptr;
	}
	case io::Message::QuitServer: {
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
		mtl_info("Received QuitServer signal over socket");
#endif
		close(fd);
		
		cornus::io::ServerLife *life = daemon->life();
		life->Lock();
		life->exit = true;
		life->Unlock();
		
		ByteArray ba;
		ba.set_msg_id(io::Message::Empty);
#ifdef CORNUS_DEBUG_SERVER_SHUTDOWN
		mtl_info("Waking up daemon to process it...");
#endif
		io::socket::SendSync(ba);
		return nullptr;
	}
	default: {}
	} // switch()
	close(fd);
	ba.to(0);
	auto *task = cornus::io::Task::From(ba, HasSecret::No);
	if (task != nullptr)
	{
		QMetaObject::invokeMethod(tasks_win, "add",
		Qt::QueuedConnection, Q_ARG(cornus::io::Task*, task));
		task->StartIO();
	}
	
	return nullptr;
}

void* PutTrashInGlobalGitignoreTh(void *args)
{
	pthread_detach(pthread_self());
	
	QString gitignore_path = trash::gitignore_global_path();
	
	if (gitignore_path.isEmpty())
	{
		gitignore_path = trash::CreateGlobalGitignore();
		trash::gitignore_global_path(&gitignore_path);
		mtl_printq2("New global gitignore ", trash::gitignore_global_path());
	} else {
		MTL_CHECK_ARG(io::EnsureRegularFile(gitignore_path), nullptr);
	}
	
	if (!io::FileContentsContains(gitignore_path, trash::basename_regex())) {
		MTL_CHECK_ARG(trash::AddTrashNameToGitignore(gitignore_path), nullptr);
		mtl_info("Added %s to global gitignore", qPrintable(trash::basename_regex()));
	}
	
	return nullptr;
}

/* static int setup_unix_signal_handlers()
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
*/
} // cornus::

int main(int argc, char *argv[])
{
	QApplication qapp(argc, argv);
	qRegisterMetaType<cornus::io::Task*>();
	qRegisterMetaType<cornus::ByteArray*>();
	
	const pid_t pid = getpid();
	printf("PID: %ld\n", i64(pid));
	
	//setup_unix_signal_handlers();
	//new cornus::MyDaemon();
	auto *daemon = new cornus::io::Daemon();
	cornus::io::NewThread(cornus::ListenTh, daemon);
	cornus::io::NewThread(cornus::PutTrashInGlobalGitignoreTh, NULL);
	cornus::io::ServerLife *life = daemon->life();
	int ret;
	while (true)
	{
		ret = qapp.exec();
		life->Lock();
		cbool should_exit = life->exit;
		life->Unlock();
		if (should_exit)
			break;
	}
	delete daemon;
	return ret;
}

