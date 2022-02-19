#include "ProcessRequest.hpp"

#include "../AutoDelete.hh"
#include "Daemon.hpp"
#include "decl.hxx"
#include "io.hh"
#include "Task.hpp"
#include "../gui/TasksWin.hpp"

namespace cornus::io {
const auto ConnectionType = Qt::BlockingQueuedConnection;

void ProcessRequest::run()
{
	cornus::ByteArray ba;
	int fd = args_->fd;
	cornus::io::Daemon *daemon = args_->daemon;
	gui::TasksWin *tasks_win = daemon->tasks_win();
	delete args_;
	args_ = nullptr;
	MTL_CHECK_VOID(ba.Receive(fd, CloseSocket::No));
	const auto msg_int = ba.next_u32() & ~(io::MessageBitsMask << io::MessageBitsStartAt);
	const auto msg = static_cast<io::Message>(msg_int);
	switch (msg)
	{
	case io::Message::CheckAlive: {
		close(fd);
		return;
	}
	case io::Message::CopyToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(daemon, "CopyURLsToClipboard",
		ConnectionType, Q_ARG(ByteArray*, &ba));
		return;
	}
	case io::Message::CutToClipboard: {
		close(fd);
		QMetaObject::invokeMethod(daemon, "CutURLsToClipboard",
		ConnectionType, Q_ARG(ByteArray*, &ba));
		return;
	}
	case io::Message::EmptyTrashRecursively: {
		close(fd);
		QString dir_path = ba.next_string();
		QMetaObject::invokeMethod(daemon, "EmptyTrashRecursively",
		ConnectionType, Q_ARG(QString, dir_path), Q_ARG(bool, true));
		return;
	}
	case io::Message::SendOpenWithList: {
		QString mime = ba.next_string();
		QMetaObject::invokeMethod(daemon, "SendOpenWithList",
		ConnectionType, Q_ARG(QString, mime), Q_ARG(int, fd));
		return;
	}
	case io::Message::SendDefaultDesktopFileForFullPath: {
		QMetaObject::invokeMethod(daemon, "SendDefaultDesktopFileForFullPath",
		ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
		return;
	}
	case io::Message::SendDesktopFilesById: {
		QMetaObject::invokeMethod(daemon, "SendDesktopFilesById",
		ConnectionType, Q_ARG(ByteArray*, &ba), Q_ARG(int, fd));
		return;
	}
	case io::Message::SendAllDesktopFiles: {
		QMetaObject::invokeMethod(daemon, "SendAllDesktopFiles",
		ConnectionType, Q_ARG(int, fd));
		return;
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
		return;
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
}

} // cornus::io::
