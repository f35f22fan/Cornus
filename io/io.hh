#pragma once

#include "../ByteArray.hpp"
#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../gui/decl.hxx"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <mutex>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <sys/sysmacros.h>

#include <QStringRef>
#include <QVector>

namespace cornus::io {

namespace FileBits {
	const u8 Selected = 1u << 0;
}

const f64 KiB = 1024;
const f64 MiB = KiB * 1024;
const f64 GiB = MiB * 1024;
const f64 TiB = GiB * 1024;

static const mode_t DirPermissions = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
static const mode_t FilePermissions = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

enum ListOptions : u8 {
	HiddenFiles = 1u << 0,
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

struct Files {
	Files() {}
	~Files() {}
	QVector<io::File*> vec;
	QString dir_path;
	SortingOrder sorting_order;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	i32 dir_id = 0; // for inotify/epoll
	bool show_hidden_files = false;
private:
	NO_ASSIGN_COPY_MOVE(Files);
};

enum class Err : u8 {
	Ok = 0,
	Access, // permission denied
	Perm, // operation not permitted
	IO, // I/O error
	CircularSymlink,
	Other
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
	QVector<FileID> chain_ids_; // for detecting circular symlinks
	QVector<QString> chain_paths_;
	mode_t mode = 0;
	FileType type = FileType::Unknown;
	i8 cycles = 1; // @cycles is set negative when circular symlink detected.
	static const i8 MaxCycles = 6;
	
	LinkTarget* Clone() {
		LinkTarget *p = new LinkTarget();
		p->path = path;
		p->chain_ids_ = chain_ids_;
		p->chain_paths_ = chain_paths_;
		p->type = type;
		p->cycles = cycles;
		return p;
	}
};

typedef bool (*FilterFunc)(const QString &dir_path, const QString &name);

void Delete(io::File *file);

bool
EnsureDir(const QString &dir_path, const QString &subdir);

bool
FileExistsCstr(const char *path, FileType *file_type = nullptr);

inline bool
FileExists(const QString &path, FileType *file_type = nullptr) {
	auto ba = path.toLocal8Bit();
	return FileExistsCstr(ba.data(), file_type);
}

const char*
FileTypeToString(const FileType t);

void
FillInStx(io::File &file, const struct statx &st, const QString *name);

QString
FloatToString(const float number, const int precision);

QStringRef
GetFilenameExtension(const QString &name);

inline bool IsNearlyEqual(double x, double y);

io::Err
ListFileNames(const QString &full_dir_path, QVector<QString> &vec);

io::Err
ListFiles(Files &files, FilterFunc ff = nullptr);

io::Err
MapPosixError(int e);

inline FileType
MapPosixTypeToLocal(const mode_t mode) {
	switch (mode & S_IFMT) {
	case S_IFREG: return FileType::Regular;
	case S_IFDIR: return FileType::Dir;
	case S_IFLNK: return FileType::Symlink;
	case S_IFBLK: return FileType::BlockDevice;
	case S_IFCHR: return FileType::CharDevice;
	case S_IFIFO: return FileType::Pipe;
	case S_IFSOCK: return FileType::Socket;
	default: return FileType::Unknown;
	}
}

io::Err
ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const i64 read_max = -1);

void
ReadLink(const char *file_path, LinkTarget &link_target,
	const QString &parent_dir);

bool ReloadMeta(io::File &file);

Err SameFiles(const QString &path1, const QString &path2, bool &same);

bool
SortFiles(File *a, File *b);

isize TryReadFile(const QString &full_path, char *buf,
	const i64 how_much, ExecInfo *info = nullptr);

io::Err
WriteToFile(const QString &full_path, const char *data, const i64 size);

} // cornus::io::::
