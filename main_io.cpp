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
#include "io/io.hh"
#include "io/Daemon.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "trash.hh"
#include "gui/TasksWin.hpp"
#include "MyDaemon.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);

///#define CORNUS_DEBUG_SERVER_SHUTDOWN

namespace cornus {
const auto ConnectionType = Qt::BlockingQueuedConnection;
struct args_data {
	io::Daemon *daemon = nullptr;
	int fd = -1;
};

void* PutTrashInGlobalGitignore(void *args)
{
	pthread_detach(pthread_self());
	
	QString gitignore_path = trash::gitignore_global_path();
	
	if (gitignore_path.isEmpty())
	{
		gitignore_path = trash::CreateGlobalGitignore();
		trash::gitignore_global_path(&gitignore_path);
		mtl_printq2("New global gitignore ", trash::gitignore_global_path());
	} else {
		RET_IF(io::EnsureRegularFile(gitignore_path), false, NULL);
	}
	
	if (!io::FileContentsContains(gitignore_path, trash::basename_regex())) {
		RET_IF(trash::AddTrashNameToGitignore(gitignore_path), false, NULL);
		mtl_info("Added %s to global gitignore", qPrintable(trash::basename_regex()));
	}
	
	return nullptr;
}

void* ProcessRequest(void *ptr)
{
	pthread_detach(pthread_self());
	cornus::ByteArray ba;
	args_data *args = (args_data*)ptr;
	int fd = args->fd;
	cornus::io::Daemon *daemon = args->daemon;
	gui::TasksWin *tasks_win = daemon->tasks_win();
	delete args;
	args = nullptr;
	RET_IF(ba.Receive(fd, CloseSocket::No), false, NULL);
	const auto msg_int = ba.next_u32() & ~(io::MessageBitsMask << io::MessageBitsStartAt);
	const auto msg = static_cast<io::Message>(msg_int);
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
			ConnectionType, Q_ARG(QString, mime), Q_ARG(int, fd));
		return nullptr;
	}
	case io::Message::SendDefaultDesktopFileForFullPath: {
		QMetaObject::invokeMethod(daemon, "SendDefaultDesktopFileForFullPath",
			ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
		return nullptr;
	}
	case io::Message::SendDesktopFilesById: {
		QMetaObject::invokeMethod(daemon, "SendDesktopFilesById",
			ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
		return nullptr;
	}
	case io::Message::SendAllDesktopFiles: {
		QMetaObject::invokeMethod(daemon, "SendAllDesktopFiles",
			ConnectionType, Q_ARG(int, fd));
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
			ConnectionType, Q_ARG(cornus::io::Task*, task));
		task->StartIO();
	}
	
	return nullptr;
}

void* ListenTh(void *args)
{
	pthread_detach(pthread_self());
	auto *daemon = (io::Daemon*)args;
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
		int client_fd = accept(daemon_sock_fd, NULL, NULL);
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
		
		auto *args = new args_data();
		args->fd = client_fd;
		args->daemon = daemon;
		if (!io::NewThread(ProcessRequest, args))
			break;
	}
	
	close(daemon_sock_fd);
	QMetaObject::invokeMethod(daemon, "QuitGuiApp", ConnectionType);

	return nullptr;
}
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

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	printf("%s PID: %ld\n", argv[0], i64(pid));
	QApplication qapp(argc, argv);
	qRegisterMetaType<cornus::io::Task*>();
	qRegisterMetaType<cornus::ByteArray*>();
	//setup_unix_signal_handlers();
	//new cornus::MyDaemon();
	
	auto *daemon = new cornus::io::Daemon();
	
	{
		cornus::io::NewThread(cornus::ListenTh, daemon);
		cornus::io::NewThread(cornus::PutTrashInGlobalGitignore, NULL);
	}

	cornus::io::ServerLife *life = daemon->life();
	int ret;
	while (true)
	{
		ret = qapp.exec();
		life->Lock();
		const bool do_exit = life->exit;
		life->Unlock();
		if (do_exit) {
			break;
		}
	}
	delete daemon;

	return ret;
}

