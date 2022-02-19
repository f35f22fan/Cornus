#include "ListenThread.hpp"

#include "Daemon.hpp"
#include "socket.hh"

#include "../AutoDelete.hh"
#include "decl.hxx"
#include "io.hh"
#include "ProcessRequest.hpp"
#include "Task.hpp"
#include "../gui/TasksWin.hpp"

namespace cornus::io {
const auto ConnectionType = Qt::BlockingQueuedConnection;

void ListenThread::run()
{
	io::ServerLife *life = daemon_->life();
	
	int daemon_sock_fd = io::socket::Daemon(cornus::SocketPath, PrintErrors::No);
	if (daemon_sock_fd == -1)
	{
		mtl_info("Another cornus_io is running. Exiting.");
		QMetaObject::invokeMethod(daemon_, "QuitGuiApp", ConnectionType);
		return;
	}
	
	while (true)
	{
		const int client_fd = accept(daemon_sock_fd, NULL, NULL);
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
		args->daemon = daemon_;
//		if (!io::NewThread(ProcessRequest, args))
//			break;
		ProcessRequest *request = new ProcessRequest(args);
		request->start();
	}
	
	close(daemon_sock_fd);
	QMetaObject::invokeMethod(daemon_, "QuitGuiApp", ConnectionType);
}

} // cornus::io::
