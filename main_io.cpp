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
#include "io/socket.hh"

void* ProcessRequest(void *args)
{
	pthread_detach(pthread_self());
	cornus::ByteArray ba;
	int fd = MTL_PTR_TO_INT(args);
	ba.Receive(fd);
	
	mtl_info("u8: %u", ba.next_u8());
	mtl_info("i8: %d", ba.next_i8());
	mtl_printq2("string: ", ba.next_string());
	mtl_info("i32: %d", ba.next_i32());
	
	return nullptr;
}


void* StartServerSocket(void*)
{
	pthread_detach(pthread_self());
	int server_sock_fd = cornus::io::socket::Server(cornus::SocketPath);
	CHECK_TRUE_NULL((server_sock_fd != -1));
	pthread_t th;
	
	while (true) {
		int client_fd = accept(server_sock_fd, NULL, NULL);
		
		if (client_fd == -1) {
			mtl_status(errno);
			break;
		}
		
		void *arg = MTL_INT_TO_PTR(client_fd);
		int status = pthread_create(&th, NULL, ProcessRequest, arg);
		if (status != 0) {
			mtl_status(status);
			break;
		}
	}
	
	mtl_info("Server socket closed");
	return nullptr;
}

int main(int argc, char *argv[]) {
	QApplication qapp(argc, argv);
	
//	cornus::App app;
//	app.show();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, StartServerSocket, NULL);
	if (status != 0)
		mtl_status(status);
	
	pthread_join(th, NULL);
	
	return 0;
	//return qapp.exec();
}

