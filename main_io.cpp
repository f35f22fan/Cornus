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
#include "io/ListenThread.hpp"
#include "io/socket.hh"
#include "io/Task.hpp"
#include "trash.hh"
#include "gui/TasksWin.hpp"
#include "MyDaemon.hpp"

Q_DECLARE_METATYPE(cornus::io::Task*);

///#define CORNUS_DEBUG_SERVER_SHUTDOWN

namespace cornus {

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
		MTL_CHECK_ARG(io::EnsureRegularFile(gitignore_path), nullptr);
	}
	
	if (!io::FileContentsContains(gitignore_path, trash::basename_regex())) {
		MTL_CHECK_ARG(trash::AddTrashNameToGitignore(gitignore_path), nullptr);
		mtl_info("Added %s to global gitignore", qPrintable(trash::basename_regex()));
	}
	
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
	QApplication qapp(argc, argv);
	pid_t pid = getpid();
	printf("%s PID: %ld\n", argv[0], i64(pid));
	
	qRegisterMetaType<cornus::io::Task*>();
	qRegisterMetaType<cornus::ByteArray*>();
	//setup_unix_signal_handlers();
	//new cornus::MyDaemon();
	auto *daemon = new cornus::io::Daemon();
	{
		//cornus::io::NewThread(cornus::ListenTh, daemon);
		auto *workerThread = new cornus::io::ListenThread(daemon);
//		connect(workerThread, &WorkerThread::resultReady, this, &MyObject::handleResults);
//		connect(workerThread, &WorkerThread::finished, workerThread, &QObject::deleteLater);
		workerThread->start();
	}
	
	cornus::io::NewThread(cornus::PutTrashInGlobalGitignore, NULL);
	
	cornus::io::ServerLife *life = daemon->life();
	int ret;
	while (true)
	{
		ret = qapp.exec();
		life->Lock();
		const bool should_exit = life->exit;
		life->Unlock();
		if (should_exit)
			break;
	}
	delete daemon;
	return ret;
}

