#pragma once

#include "types.hxx"
#include <sys/stat.h>
#include <QString>
#include <QVector>
#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus {
class App;
class ByteArray;
class ElapsedTimer;
class History;
class MutexGuard;

// #define DEBUG_CLIPBOARD

const QString AppIconPath = QLatin1String(":/resources/cornus.webp");
const char *const SocketPath = "\0cornus_socket";

enum class FileAs: i8 {
	URL,
	FilePath,
};

enum class ClipboardAction: i8 {
	None,
	Cut,
	Copy,
	Link,
	Paste,
};

struct Clipboard {
	QVector<QString> file_paths;
	ClipboardAction action = ClipboardAction::None;
	
	inline bool has_files() const {
		return action != ClipboardAction::None && !file_paths.isEmpty();
	}
};

enum class PartitionEventType: u8 {
	None = 0,
	Mount,
	Unmount,
};

struct PartitionEvent {
	QString dev_path;
	QString mount_path;
	QString fs;
	PartitionEventType type = PartitionEventType::None;
};
}

Q_DECLARE_METATYPE(cornus::PartitionEvent*);

namespace cornus {

namespace ExecType {
	const u16 None = 0;
	const u16 Elf = 1u << 0;
	const u16 Script = 1u << 1;
	/// Additional script info:
	const u16 ScriptBash = 1u << 2;
	const u16 ScriptPython = 1u << 3;
	const u16 ScriptJs = 1u << 4;
	const u16 ScriptSh = 1u << 5;
	const u16 ScriptBat = 1u << 6;
}

struct ExecInfo {
	mode_t mode = 0;
	u16 type = 0;
	
	bool is_elf() const { return type & ExecType::Elf; }
	bool is_script() const { return type & ExecType::Script; }
	bool is_script_sh() const { return type & ExecType::ScriptSh; }
	bool is_script_bash() const { return type & ExecType::ScriptBash; }
	bool is_script_python() const { return type & ExecType::ScriptPython; }
	bool has_exec_bit() const { return mode & (S_IXUSR|S_IXGRP|S_IXOTH); }
	bool is_regular_file() const { return S_ISREG(mode); }
	bool is_symlink() const { return S_ISLNK(mode); }
};

}
