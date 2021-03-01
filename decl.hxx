#pragma once

#include "types.hxx"
#include <sys/stat.h>
#include <QString>
#include <QVector>
#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus {
class App;
class ByteArray;
class DesktopFile;
class ElapsedTimer;
class ExecInfo;
class History;
class MutexGuard;
class Prefs;

const QString AppIconPath = QLatin1String(":/io.github.Cornus.webp");
const char *const SocketPath = "\0cornus_socket";

enum class StringCase: i8 {
	Lower,
	AsIs,
};

enum class Direction: i8 {
	Next,
	Prev,
};

enum class Action: i8 {
	None = 0,
	Up,
	Back,
	Forward,
	Reload,
	To,
};

enum class Reload: i8 {
	No = 0,
	Yes,
};

enum class Processed: i8 {
	No = 0,
	Yes = 1,
};

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

enum class ClipboardType: i8 {
	None,
	Nautilus,
	KDE
};

struct Clipboard {
	QVector<QString> file_paths;
	ClipboardAction action = ClipboardAction::None;
	ClipboardType type = ClipboardType::None;
	
	inline bool has_files() const {
		return action != ClipboardAction::None && !file_paths.isEmpty();
	}
	
	inline int file_count() const { 
		return file_paths.size();
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

