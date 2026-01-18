#pragma once

#include "types.hxx"
#include <sys/stat.h>
#include <QHash>
#include <QString>
#include <QVector>
#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus {
class App;
class ByteArray;
class DesktopFile;
class ElapsedTimer;
class ExecInfo;
class Hid;
class History;
class Media;
class MutexGuard;
class MyDaemon;
class Prefs;
class TreeItems;

const QString AppIconPath = QLatin1String(":/cornus.mas.png");
static const char *const SocketPath = "\0cornus_socket";
static const char *const RootSocketPath = "\0cornus_socket_root";

using TabId = i64;
using DirId = i32;
using FileId = i32;
using FilesId = i64;

struct PathAndMode {
	QString path; // expected to be full path
	mode_t mode;
	
	bool operator==(const PathAndMode &rhs) const {
		return (S_IFMT & mode) == (S_IFMT & rhs.mode) && path == rhs.path;
	}
};

using EfaType = u8;
enum class Efa: EfaType {
	None = 0,
	Thumbnail = 1,
	Text = 1 << 1,
	All = Thumbnail | Text,
};

inline bool EfaContains(Efa a, Efa b) {
	return (static_cast<EfaType>(a) & static_cast<EfaType>(b)) != 0;
}

inline Efa operator & (Efa a, Efa b) {
	return static_cast<Efa>(static_cast<EfaType>(a) & static_cast<EfaType>(b));
}

inline Efa operator | (Efa a, Efa b) {
	return static_cast<Efa>(static_cast<EfaType>(a) | static_cast<EfaType>(b));
}

inline Efa& operator |= (Efa &a, const Efa &b) {
	a = a | b;
	return a;
}

inline Efa operator ~ (Efa a) {
	return static_cast<Efa>(~(static_cast<EfaType>(a)));
}

inline Efa& operator &= (Efa &a, const Efa &b) {
	a = static_cast<Efa>(static_cast<EfaType>(a) & static_cast<EfaType>(b));
	return a;
}

enum class Op: i8 {
	Add,
	Remove,
	Invert,
};

enum class DeleteSubFolders: i8 {
	Yes,
	No
};

enum class DeleteTopFolder: i8 {
	Yes,
	No
};

enum class Exists: i8 {
	Yes,
	No
};

enum class Lock: i8 {
	Yes,
	No
};

enum class TopLevel: i8 {
	Browser,
	Editor,
	ImageViewer
};

enum class FocusView: i8 {
	Yes,
	No
};

struct Range {
	int min = -1;
	int max = -1;
	
	bool is_valid() const { return min != -1 && max != -1; }
	static Range Invalid() { return Range{}; }
};

enum class Path: i8 {
	OnlyName,
	Full
};

enum class PickedBy: i8 {
	Icon,
	VisibleName,
};

enum class ForceDropToCurrentDir: i8 {
	Yes,
	No
};

enum class From: i8 {
	Start,
	CurrentPosition
};

enum class ExactSize: i8 {
	Yes,
	No
};

enum class CanRelyOnStatxSize: i8 {
	Yes,
	No
};

enum class SameDir: i8 {
	Yes,
	No
};

enum class DeselectOthers: i8 {
	Yes,
	No
};

enum class WhichFiles: i8 {
	Selected,
	All
};

enum class Selected: i8 {
	Yes,
	No
};

enum class FileCountChanged: i8 {
	Yes,
	No
};

enum class NewState: i8 {
	Set,
	AboutToSet,
};

enum class TempDir: i8 {
	Yes,
	No
};

enum class Repaint: i8 {
	IfViewIsCurrent,
	No,
};

enum class CanOverwrite: i8 {
	Yes,
	No
};

using IOActionType = i8;
enum class IOAction: IOActionType {
	None,
	AutoRenameAll,
	SkipAll,
	OverwriteAll,
};

inline uint qHash(IOAction key, uint seed)
{
	return ::qHash(static_cast<IOActionType>(key), seed);
}

struct HashInfo {
	u64 num = 0;
	QString hash_str;
	
	bool valid() const { return !hash_str.isEmpty(); }
};

enum class HasSecret: i8 {
	Yes,
	No
};

enum class NamesAreLowerCase: i8 {
	Yes,
	No
};

enum class ShiftPressed: i8 {
	Yes,
	No
};

enum class FirstTime: i8 {
	No,
	Yes
};

enum class Present: i8 {
	Yes,
	No
};

enum class StringLength: i8 {
	Short,
	Normal
};

enum class InsertPlace: i8 {
	AtEnd,
	Sorted,
	AtStart
};

enum class Device: i8 {
	None,
	Disk,
	Partition
};

enum class DeviceAction: i8 {
	None,
	Added,
	Removed
};

enum class VDirection: i8 {
	Up,
	Down
};

enum class ListDirOption: i8 {
	IncludeLinksToDirs,
	DontIncludeLinksToDir
};

enum class Zoom: i8 {
	In,
	Out,
	Reset
};

enum class LinkType: i8 {
	Absolute,
	Relative
};

enum class ThemeType: i8 {
	None,
	Light,
	Dark,
};

enum class Clone: i8 {
	Yes,
	No,
};

enum class PrintErrors: i8 {
	Yes,
	No
};

enum class Bool: i8 {
	Yes,
	No,
	None
};

enum class RunAction: i8 {
	Run,
	Open,
	ChooseBasedOnExecBit
};

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

enum class ClipboardAction: u8 {
	None = 0,
	Cut,
	Copy,
	Paste,
	PasteLink,
};

enum class ClipboardType: u8 {
	None = 0,
	Gnome,
	KDE
};

struct ClipboardData {
	QList<QUrl> urls;
	ClipboardAction action = ClipboardAction::None;
	ClipboardType type = ClipboardType::None;
};

enum class PartitionEventType: i8 {
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

struct PendingCommand {
	static const u8 ExpiredBit = 1u << 0;
	
	u8 bits_ = 0;
	PartitionEventType evt = PartitionEventType::None;
	time_t expires = 0;
	QString fs_uuid;
	QString path;
	
	static inline PendingCommand New(const PartitionEventType evt,
		const QString &fs_uuid, const QString &path)
	{
		return PendingCommand {
			.evt = evt,
			.expires = time(NULL) + 30,
			.fs_uuid = fs_uuid,
			.path = path
		};
	}
	
	inline bool expired_bit() const { return bits_ & ExpiredBit; }
	inline void expired_bit(const bool flag) {
		if (flag)
			bits_ |= ExpiredBit;
		else
			bits_ &= ~ExpiredBit;
	}
};

inline bool DoublesEqual(const double A, const double B, const double epsilon)
{
	const auto diff = std::abs(A - B);
	return (diff < epsilon);
}

} // cornus::

Q_DECLARE_METATYPE(cornus::PartitionEvent*);
Q_DECLARE_METATYPE(cornus::Device);
Q_DECLARE_METATYPE(cornus::DeviceAction);
