#include "socket.hh"

#include "../ByteArray.hpp"
#include "io.hh"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRandomGenerator>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace cornus::io::socket {

void* AutoLoadIODaemonIfNeeded(void *arg)
{
	pthread_detach(pthread_self());
	const char *socket_p = (const char*) arg;
	ByteArray check_alive_ba;
	check_alive_ba.set_msg_id(io::Message::CheckAlive);
	
	if (io::socket::SendSync(check_alive_ba, socket_p)) {
		return nullptr; // daemon is online
	}
	
	const QString fn = QLatin1String("/.cornus_check_online_excl_");
	QString excl_file_path = QDir::homePath() + fn;
	auto excl_ba = excl_file_path.toLocal8Bit();
	int fd = open(excl_ba.data(), O_EXCL | O_CREAT, 0x777);
	
	const QString daemon_dir_path = QCoreApplication::applicationDirPath();
	const QString app_to_execute = daemon_dir_path + QLatin1String("/cornus_io");
	
	if (fd == -1) {
		mtl_info("Some app already trying to start %s : %s",
			qPrintable(app_to_execute), socket_p + 1);
		return nullptr;
	}
	
	QStringList arguments;
	QProcess::startDetached(app_to_execute, arguments, daemon_dir_path);

	// wait till daemon is started:
	for (int sec = 0; sec < 7; sec++)
	{
		if (io::socket::SendSync(check_alive_ba, socket_p)) {
			break;
		}
		sleep(1);
	}

	int status = remove(excl_ba.data());
	if (status != 0)
		mtl_status(errno);
	
	return nullptr;
}

void AutoLoadRegularIODaemon()
{
	pthread_t th;
	int status = pthread_create(&th, NULL, AutoLoadIODaemonIfNeeded, (void*)cornus::SocketPath);
	if (status != 0)
		mtl_status(status);
}

struct SendData {
	const char *addr = nullptr;
	bool delete_addr = false;
	ByteArray *ba = nullptr;
};

void* SendTh(void *args)
{
	pthread_detach(pthread_self());
	SendData *data = (SendData*) args;
	int fd = io::socket::Client(data->addr);
	
	if (!data->ba->Send(fd))
		mtl_warn("Failed to send bytes");
	
	delete data->ba;
	if (data->delete_addr)
		delete data->addr;
	delete data;
	
	return nullptr;
}

void FillIn(struct sockaddr_un &addr, const char *addr_str)
{
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (addr_str[0] == '\0') {
		*addr.sun_path = '\0';
		strncpy(addr.sun_path + 1, addr_str + 1, sizeof(addr.sun_path) - 2);
	} else {
		strncpy(addr.sun_path, addr_str, sizeof(addr.sun_path) - 1);
	}
}

int Client(const char *addr_str)
{
	int sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		mtl_status(errno);
		return -1;
	}
	
	struct sockaddr_un addr;
	FillIn(addr, addr_str);
	
	if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		return -1;
	}
	
	return sock_fd;
}

int Daemon(const char *addr_str, const PrintErrors pe)
{
	int sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1)
	{
		if (pe == PrintErrors::Yes)
			mtl_status(errno);
		return -1;
	}
	
	struct sockaddr_un addr;
	FillIn(addr, addr_str);
	
	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		if (pe == PrintErrors::Yes)
			mtl_status(errno);
		return -1;
	}
	
	if (listen(sock_fd, 5) == -1)
	{
		if (pe == PrintErrors::Yes)
			mtl_status(errno);
		return -1;
	}
	
	return sock_fd;
}

bool SendAsync(ByteArray *ba, const char *socket_path, const bool delete_path)
{
	auto *data = new SendData();
	data->ba = ba;
	data->addr = socket_path;
	data->delete_addr = delete_path;
	pthread_t th;
	int status = pthread_create(&th, NULL, SendTh, data);
	if (status != 0)
	{
		mtl_status(status);
		delete data->ba;
		if (data->delete_addr)
			delete data->addr;
		
		delete data;
	}
	
	return status == 0;
}

void SendQuitSignalToServer()
{
	ByteArray *ba = new ByteArray();
	ba->set_msg_id(io::Message::QuitServer);
	SendAsync(ba);
}

bool SendSync(const ByteArray &ba, const char *socket_path)
{
	int fd = io::socket::Client(socket_path);
	return ba.Send(fd);
}

}
