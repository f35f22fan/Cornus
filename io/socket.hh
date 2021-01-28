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

using MsgType = u32;
enum MsgBits: MsgType {
	Copy = 1u << 0,
	Move = 1u << 1,
	Link = 1u << 2,
};

inline MsgType MsgFlagsFor(const Qt::DropAction action)
{
	io::socket::MsgType bits = 0;
	if (action & Qt::CopyAction)
		bits |= io::socket::MsgBits::Copy;
	if (action & Qt::MoveAction)
		bits |= io::socket::MsgBits::Move;
	if (action & Qt::LinkAction)
		bits |= io::socket::MsgBits::Link;
	return bits;
}

int Client(const char *addr_str);
int Server(const char *addr_str);

void SendAsync(ByteArray *ba, const char *socket_path = nullptr,
	const bool delete_path = false);


}
