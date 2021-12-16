#pragma once

#include <QDropEvent>

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../decl.hxx"
#include "../err.hpp"

namespace cornus::io::socket {

void AutoLoadRegularIODaemon();

int Client(const char *addr_str = cornus::SocketPath);

int Daemon(const char *addr_str = cornus::SocketPath);

bool SendAsync(ByteArray *ba, const char *socket_path = cornus::SocketPath,
	const bool delete_path = false);

void SendQuitSignalToServer();

bool SendSync(const ByteArray &ba, const char *socket_path = cornus::SocketPath);

}
