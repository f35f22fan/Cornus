#pragma once

#include "../ByteArray.hpp"
#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../gui/decl.hxx"

#include <chrono>
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

#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus::io {
struct FilesData {
	FilesData() {}
	~FilesData() {}
	FilesData(const FilesData &rhs) = delete;
	std::chrono::time_point<std::chrono::steady_clock> start_time;
	QVector<io::File*> vec;
	QString processed_dir_path;
	QString unprocessed_dir_path;
	SortingOrder sorting_order;
	i32 dir_id = 0;/// for inotify/epoll
	bool show_hidden_files = false;
	bool widgets_created = false;
	QString scroll_to_and_select;
};

struct Files {
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	FilesData data = {};
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_warn("pthreads_mutex_lock: %s", strerror(status));
		return status;
	}
	
	inline int Unlock() {
		return pthread_mutex_unlock(&mutex);
	}
};

struct CountRecursiveInfo {
	i64 size = 0;
	i64 size_without_dirs_meta = 0;
	i32 file_count = 0;
	i32 dir_count = -1; // -1 to exclude counting parent folder
};
}
Q_DECLARE_METATYPE(cornus::io::FilesData*);
Q_DECLARE_METATYPE(cornus::io::CountRecursiveInfo*);

namespace cornus::io {

struct CountFolderData {
	io::CountRecursiveInfo info = {};
	QString full_path;
	QString err;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	bool thread_has_quit = false;
	bool app_has_quit = false;
	
	inline void Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
	
	inline void Unlock() {
		int status = pthread_mutex_unlock(&mutex);
		if (status != 0)
			mtl_status(status);
	}
};

bool CountSizeRecursive(const QString &path, struct statx &stx,
	CountRecursiveInfo &info);

bool CountSizeRecursiveTh(const QString &path, struct statx &stx,
	CountFolderData &data);

void Delete(io::File *file);

bool
EnsureDir(const QString &dir_path, const QString &subdir);

bool ExpandLinksInDirPath(QString &unprocessed_dir_path, QString &processed_dir_path);

bool
FileExistsCstr(const char *path, FileType *file_type = nullptr);

inline bool
FileExists(const QString &path, FileType *file_type = nullptr) {
	auto ba = path.toLocal8Bit();
	return FileExistsCstr(ba.data(), file_type);
}

io::File*
FileFromPath(const QString &full_path, io::Err *ret_error = nullptr);

const char*
FileTypeToString(const FileType t);

void
FillInStx(io::File &file, const struct statx &st, const QString *name);

QString
FloatToString(const float number, const int precision);

QStringRef
GetFilenameExtension(const QString &name);

QStringRef
GetFileNameOfFullPath(const QString &full_path);

inline bool IsNearlyEqual(double x, double y);

io::Err
ListFileNames(const QString &full_dir_path, QVector<QString> &vec);

io::Err
ListFiles(FilesData &data, Files *ptr, FilterFunc ff = nullptr);

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
	const i64 read_max = -1, mode_t *ret_mode = nullptr);

bool
ReadLink(const char *file_path, LinkTarget &link_target, const QString &parent_dir);

bool
ReadLinkSimple(const char *file_path, QString &result);

bool ReloadMeta(io::File &file);

bool SameFiles(const QString &path1, const QString &path2,
	io::Err *ret_error = nullptr);

QString
SizeToString(const i64 sz, const bool short_version = false);

bool
SortFiles(File *a, File *b);

isize TryReadFile(const QString &full_path, char *buf,
	const i64 how_much, ExecInfo *info = nullptr);

io::Err
WriteToFile(const QString &full_path, const char *data, const i64 size,
	mode_t *custom_mode = nullptr);

} // cornus::io::::
