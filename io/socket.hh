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
enum class MsgBits: MsgType {
	None = 0,
	CheckAlive,
	QuitServer,
	SendOpenWithList,
	SendDefaultDesktopFileForFullPath,
	SendDesktopFilesById,
	SendAllDesktopFiles,
	CopyToClipboard,
	CutToClipboard,
	
	Copy = 1u << 29, // copies files
	DontTryAtomicMove = 1u << 30, // moves with rename()
	Move = 1u << 31, // moves by copying to new dir and deleting old ones
};

inline MsgBits operator | (MsgBits a, MsgBits b) {
	return static_cast<MsgBits>(static_cast<MsgType>(a) | static_cast<MsgType>(b));
}

inline MsgBits& operator |= (MsgBits &a, const MsgBits &b) {
	a = a | b;
	return a;
}

inline MsgBits operator ~ (MsgBits a) {
	return static_cast<MsgBits>(~(static_cast<MsgType>(a)));
}

inline MsgBits operator & (MsgBits a, MsgBits b) {
	return static_cast<MsgBits>((static_cast<MsgType>(a) & static_cast<MsgType>(b)));
}

inline MsgBits& operator &= (MsgBits &a, const MsgBits &b) {
	a = a & b;
	return a;
}

inline MsgBits MsgFlagsFor(const Qt::DropAction action)
{
	io::socket::MsgBits bits = MsgBits::None;
	if (action & Qt::CopyAction) {
		bits |= MsgBits::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= MsgBits::Move;/// | MsgBits::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		mtl_trace();
		///bits |= MsgBits::Link;
	}
	return bits;
}

inline MsgBits MsgFlagsForMany(const Qt::DropActions action)
{
	io::socket::MsgBits bits = MsgBits::None;
	if (action & Qt::CopyAction) {
		bits |= MsgBits::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= MsgBits::Move;/// | MsgBits::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		mtl_trace();
		///bits |= MsgBits::Link;
	}
	return bits;
}

int Client(const char *addr_str = cornus::SocketPath);
int Server(const char *addr_str = cornus::SocketPath);

void SendAsync(ByteArray *ba, const char *socket_path = nullptr,
	const bool delete_path = false);

void SendQuitSignalToServer();

bool SendSync(const ByteArray &ba, const char *socket_path = nullptr);
}
