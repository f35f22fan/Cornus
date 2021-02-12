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
	AtomicMove = 1u << 1,
	Move = 1u << 2,
	Link = 1u << 3,
	CheckAlive = 1u << 4,
	SendOpenWithList = 1u << 5,
	CopyToClipboard = 1u << 6,
	CutToClipboard = 1u << 7,
};

inline MsgType MsgFlagsFor(const Qt::DropAction action)
{
	io::socket::MsgType bits = 0;
	if (action & Qt::CopyAction) {
		bits |= MsgBits::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= MsgBits::Move | MsgBits::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		bits |= MsgBits::Link;
	}
	return bits;
}

inline MsgType MsgFlagsForMany(const Qt::DropActions action)
{
	io::socket::MsgType bits = 0;
	if (action & Qt::CopyAction) {
		bits |= MsgBits::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= MsgBits::Move | MsgBits::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		bits |= MsgBits::Link;
	}
	return bits;
}

int Client(const char *addr_str);
int Server(const char *addr_str);

void SendAsync(ByteArray *ba, const char *socket_path = nullptr,
	const bool delete_path = false);

bool SendSync(const ByteArray &ba, const char *socket_path = nullptr);
}
