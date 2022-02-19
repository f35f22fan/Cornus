#pragma once

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <pthread.h>
#include <QVector>
#include <QString>

#include "../gui/decl.hxx"
#include "../decl.hxx"

namespace cornus::io {
class AutoRemoveWatch;
class Daemon;
class DirStream;
class File;
class Files;
class FilesData;
class ListenThread;
class Notify;
class ProcessRequest;
class SaveFile;
class Task;

struct args_data {
	io::Daemon *daemon = nullptr;
	int fd = -1;
};

enum class CloseWriteEvent: i8 {
	Yes,
	No
};

enum class CountDirFiles: i8 {
	Yes,
	No
};

struct DevNum {
	i32 major;
	i32 minor;
};

struct DiskInfo {
	io::DevNum num = {-1, -1}; // dev_t->major
	QString id_model;
	QString dev_path;
};

inline bool operator==(const DevNum& lhs, const DevNum& rhs)
{
	return lhs.minor == rhs.minor && lhs.major == rhs.major;
}

struct ServerLife
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	bool exit = false;
	
	bool Lock() {
		const int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return status == 0;
	}
	
	void Unlock() {
		const int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
};
	
enum class ActUponAnswer: i8 {
	None = 0,
	Skip,
	Retry,
	Abort,
};

enum class FileExistsAnswer: i8 {
	None = 0,
	Overwrite,
	OverwriteAll,
	Skip,
	SkipAll,
	AutoRename,
	AutoRenameAll,
	Abort
};

enum class DeleteFailedAnswer: i8 {
	None = 0,
	Retry,
	Skip,
	SkipAll,
	Abort
};

enum class WriteFailedAnswer: i8 {
	None = 0,
	Retry,
	Skip,
	SkipAll,
	Abort,
};

using TaskStateT = u16;
enum class TaskState: TaskStateT {
	None =           0,
	Continue =       1u << 1,
	Working =        1u << 2,
	Pause =          1u << 3,
	Abort =          1u << 4,
	Finished =       1u << 5,
	AwaitingAnswer = 1u << 6,
	Answered =       1u << 7,
};

inline TaskState operator | (TaskState a, TaskState b) {
	return static_cast<TaskState>(static_cast<TaskStateT>(a) | static_cast<TaskStateT>(b));
}

inline bool operator & (TaskState a, TaskState b) {
	return (static_cast<TaskStateT>(a) & static_cast<TaskStateT>(b));
}

inline TaskState operator ~ (TaskState a) {
	return static_cast<TaskState>(~static_cast<TaskStateT>(a));
}

enum class FileBits: u8 {
	Empty = 0,
	Selected = 1u << 0,
	SelectedBySearch = 1u << 1,
	SelectedBySearchActive = 1u << 2,
	ActionCut = 1u << 3,
	ActionCopy = 1u << 4,
	ActionPaste = 1u << 5,
	PasteLink = 1u << 6,
};

inline FileBits operator | (FileBits a, FileBits b) {
	return static_cast<FileBits>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline FileBits& operator |= (FileBits &a, const FileBits b) {
	a = a | b;
	return a;
}

inline FileBits operator ~ (FileBits a) {
	return static_cast<FileBits>(~(static_cast<u8>(a)));
}

inline FileBits operator & (FileBits a, FileBits b) {
	return static_cast<FileBits>((static_cast<u8>(a) & static_cast<u8>(b)));
}

inline FileBits& operator &= (FileBits &a, const FileBits b) {
	a = a & b;
	return a;
}

enum ListOptions: u8 {
	HiddenFiles = 1u << 0,
};

typedef bool (*FilterFunc)(const QString &name);

const f64 KiB = 1024;
const f64 MiB = KiB * 1024;
const f64 GiB = MiB * 1024;
const f64 TiB = GiB * 1024;

static const mode_t DirPermissions = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
static const mode_t FilePermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

struct DiskFileId {
	u64 inode_number;
	u32 dev_major; // ID of device containing file, in a LAN it's unique
	u32 dev_minor;
	
	inline static DiskFileId FromStat(const struct stat &st) {
		return io::DiskFileId {
			.inode_number = st.st_ino,
			.dev_major = gnu_dev_major(st.st_dev),
			.dev_minor = gnu_dev_minor(st.st_dev),
		};
	}
	
	inline static DiskFileId FromStx(const struct statx &stx) {
		return io::DiskFileId {
			.inode_number = stx.stx_ino,
			.dev_major = stx.stx_dev_major,
			.dev_minor = stx.stx_dev_minor,
		};
	}
	
	inline bool
	operator == (const DiskFileId &rhs) const {
		return Equals(rhs);
	}
	
	inline bool
	Equals(const DiskFileId &rhs) const {
		return inode_number == rhs.inode_number &&
		dev_major == rhs.dev_major && dev_minor == rhs.dev_minor;
	}
};

struct SortingOrder {
	SortingOrder() {}
	SortingOrder(gui::Column c, bool asc): column(c), ascending(asc) {}
	SortingOrder(const SortingOrder &rhs) {
		column = rhs.column;
		ascending = rhs.ascending;
	}
	gui::Column column = gui::Column::FileName;
	bool ascending = true;
};

enum class FileType : u8 {
	Unknown = 0,
	Regular,
	Dir,
	Symlink,
	Socket,
	Pipe,
	BlockDevice,
	CharDevice
};

struct LinkTarget {
	QString path;
	QVector<DiskFileId> chain_ids_; // for detecting circular symlinks
	QVector<QString> chain_paths_;
	mode_t mode = 0;
	FileType type = FileType::Unknown;
	i8 cycles = 1; // @cycles is set negative when circular symlink detected.
	bool is_relative = false;
	static const i8 MaxCycles = 6;
	
	LinkTarget* Clone() {
		LinkTarget *p = new LinkTarget();
		p->path = path;
		p->chain_ids_ = chain_ids_;
		p->chain_paths_ = chain_paths_;
		p->type = type;
		p->is_relative = is_relative;
		p->cycles = cycles;
		return p;
	}
};

} /// namespace
