#pragma once

#include "../ByteArray.hpp"
#include "../category.hh"
#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../gui/decl.hxx"
#include "../media.hxx"
#include "../MutexGuard.hpp"

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

#include <QHash>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QMimeData>
#include <QStringRef>
#include <QVector>

namespace cornus::io {

const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;

struct Notify {
	int fd = -1;
/** Why a map? => inotify_add_watch() only adds a new watch if
there's no previous watch watching the same location, otherwise it
returns an existing descriptor which is likely used by some other code.
Therefore the following bug can happen:
if two code paths register/watch the same location thru the same
inotify instance at the same time and one of them removes the
watch - the other code path will also lose ability to watch that
same location. That's quite a subtle bug. Therefore use a map that
acts like a refcounter: when a given watch FD goes to zero it's OK
to remove it, otherwise just decrease it by 1.*/
	QMap<int, int> watches;
	pthread_mutex_t watches_mutex = PTHREAD_MUTEX_INITIALIZER;
	QVector<const char*> MaskToString(const u32 mask);
	void Close();
	void Init();
};

struct FilesData {
	static const u16 ShowHiddenFiles = 1u << 0;
	static const u16 WidgetsCreated = 1u << 1;
	static const u16 ThreadMustExit = 1u << 2;
	static const u16 ThreadExited = 1u << 3;
	
	FilesData() {}
	~FilesData() {}
	FilesData(const FilesData &rhs) = delete;
	std::chrono::time_point<std::chrono::steady_clock> start_time;
	QVector<io::File*> vec;
	QString processed_dir_path;
	QString unprocessed_dir_path;
	QString scroll_to_and_select;
	SortingOrder sorting_order;
	i32 dir_id = 0;/// for inotify/epoll
	u16 bool_ = 0;
	cornus::Action action = Action::None;
	
	bool show_hidden_files() const { return bool_ & ShowHiddenFiles; }
	void show_hidden_files(const bool flag) {
		if (flag)
			bool_ |= ShowHiddenFiles;
		else
			bool_ &= ~ShowHiddenFiles;
	}
	
	bool widgets_created() const { return bool_ & WidgetsCreated; }
	void widgets_created(const bool flag) {
		if (flag)
			bool_ |= WidgetsCreated;
		else
			bool_ &= ~WidgetsCreated;
	}
	
	bool thread_must_exit() const { return bool_ & ThreadMustExit; }
	void thread_must_exit(const bool flag) {
		if (flag)
			bool_ |= ThreadMustExit;
		else
			bool_ &= ~ThreadMustExit;
	}
	
	bool thread_exited() const { return bool_ & ThreadExited; }
	void thread_exited(const bool flag) {
		if (flag)
			bool_ |= ThreadExited;
		else
			bool_ &= ~ThreadExited;
	}
};

struct Files {
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	FilesData data = {};
	
	MutexGuard guard() const {
		return MutexGuard(&mutex);
	}
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_warn("pthreads_mutex_lock: %s", strerror(status));
		return status;
	}
	
	inline void Signal() {
		int status = pthread_cond_signal(&cond);
		if (status != 0)
			mtl_status(status);
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

media::ShortData* DecodeShort(ByteArray &ba);

int CompareStrings(const QString &a, const QString &b);

bool CopyFileFromTo(const QString &from_full_path, QString to_dir);

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

bool DirExists(const QString &full_path);

bool EnsureDir(QString dir_path, const QString &subdir);

enum class AppendSlash: i8 {
	Yes,
	No,
};

bool ExpandLinksInDirPath(QString &unprocessed_dir_path,
	QString &processed_dir_path, const AppendSlash afs = AppendSlash::Yes);

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

void GetClipboardFiles(const QMimeData &mime, Clipboard &cl);

QStringRef
GetFileNameExtension(const QString &name, QStringRef *base_name = nullptr);

QStringRef
GetFileNameOfFullPath(const QString &full_path);

Bool HasExecBit(const QString &full_path);

void InitEnvInfo(Category &desktop, QVector<QString> &search_icons_dirs,
	QVector<QString> &xdg_data_dirs,
	QHash<QString, Category> &possible_categories);

inline bool IsNearlyEqual(double x, double y);

/// lists only dir names
void ListDirNames(QString dir_path, QVector<QString> &vec,
	const ListDirOption option = ListDirOption::IncludeLinksToDirs);

io::Err /// lists files and folders
ListFileNames(const QString &full_dir_path, QVector<QString> &vec,
	FilterFunc ff = nullptr);

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

QString
NewNamePattern(const QString &filename, i32 &next);

QString PasteLinks(const QVector<QString> &full_paths, QString target_dir, QString *err);
QString PasteRelativeLinks(const QVector<QString> &full_paths, QString target_dir, QString *err);

void
ProcessMime(QString &mime);

bool ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const PrintErrors print_errors = PrintErrors::Yes,
	const i64 read_max = -1, mode_t *ret_mode = nullptr);

bool ReadLink(const char *file_path, LinkTarget &link_target, const QString &parent_dir);

bool ReadLinkSimple(const char *file_path, QString &result);

bool ReloadMeta(io::File &file, struct statx &stx, QString *dir_path = nullptr);

bool RemoveXAttr(const QString &full_path, const QString &xattr_name);

bool SameFiles(const QString &path1, const QString &path2,
	io::Err *ret_error = nullptr);

bool sd_nvme(const QString &name);
bool valid_dev_path(const QString &name);

QString SizeToString(const i64 sz, const StringLength len = StringLength::Normal);

bool SetXAttr(const QString &full_path, const QString &xattr_name,
	const ByteArray &ba);

bool SortFiles(File *a, File *b);

isize TryReadFile(const QString &full_path, char *buf,
	const i64 how_much, ExecInfo *info = nullptr);

Err WriteToFile(const QString &full_path, const char *data, const i64 size,
	mode_t *custom_mode = nullptr);

} // cornus::io::::
