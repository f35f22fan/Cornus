#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../decl.hxx"
#include "../err.hpp"

namespace cornus::io::socket {

int Client(const char *addr_str);
int Server(const char *addr_str);

void SendAsync(ByteArray *ba, const char *socket_path, bool delete_path = false);
}
