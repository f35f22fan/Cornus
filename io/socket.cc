#include "socket.hh"

#include "../ByteArray.hpp"

namespace cornus::io::socket {

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

int Server(const char *addr_str)
{
	int sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		mtl_status(errno);
		return -1;
	}
	
	struct sockaddr_un addr;
	FillIn(addr, addr_str);
	
	if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		mtl_status(errno);
		return -1;
	}
	
	if (listen(sock_fd, 5) == -1) {
		mtl_status(errno);
		return -1;
	}
	
	return sock_fd;
}

void SendAsync(ByteArray *ba, const char *socket_path, const bool delete_path)
{
	auto *data = new SendData();
	data->ba = ba;
	if (socket_path == nullptr) {
		data->addr = cornus::SocketPath;
		data->delete_addr = false;
	} else {
		data->addr = socket_path;
		data->delete_addr = delete_path;
	}
	pthread_t th;
	int status = pthread_create(&th, NULL, SendTh, data);
	if (status != 0) {
		mtl_status(status);
		
		delete data->ba;
		if (data->delete_addr)
			delete data->addr;
		delete data;
	}
}

void SendQuitSignalToServer()
{
	ByteArray *ba = new ByteArray();
	ba->set_msg_id(MsgBits::QuitServer);
	SendAsync(ba);
}

bool SendSync(const ByteArray &ba, const char *socket_path)
{
	auto *addr = (socket_path == nullptr) ? cornus::SocketPath : socket_path;
	int fd = io::socket::Client(addr);
	return ba.Send(fd);
}

}
