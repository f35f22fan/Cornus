#pragma once

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

enum class PostWrite: i8 {
	DoNothing = 0,
	FSync,
	FDataSync
};

using MessageType = u32;
enum class Message: MessageType {
	None = 0,
	CheckAlive,
	QuitServer,
	SendOpenWithList,
	SendDefaultDesktopFileForFullPath,
	SendDesktopFilesById,
	SendAllDesktopFiles,
	CopyToClipboard,
	CutToClipboard,
	DeleteFiles,
	EmptyTrashRecursively,
	MoveToTrash,
	
	Copy = 1u << 29, // copies files
	DontTryAtomicMove = 1u << 30, // moves with rename()
	Move = 1u << 31, // moves by copying to new dir and deleting old ones
};

inline Message operator | (Message a, Message b) {
	return static_cast<Message>(static_cast<MessageType>(a) | static_cast<MessageType>(b));
}

inline Message& operator |= (Message &a, const Message &b) {
	a = a | b;
	return a;
}

inline Message operator ~ (Message a) {
	return static_cast<Message>(~(static_cast<MessageType>(a)));
}

inline Message operator & (Message a, Message b) {
	return static_cast<Message>((static_cast<MessageType>(a) & static_cast<MessageType>(b)));
}

inline Message& operator &= (Message &a, const Message &b) {
	a = a & b;
	return a;
}

inline Message MessageFor(const Qt::DropAction action)
{
	io::Message bits = Message::None;
	if (action & Qt::CopyAction) {
		bits |= Message::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= Message::Move;/// | Message::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		mtl_trace();
		///bits |= Message::Link;
	}
	return bits;
}

inline Message MessageForMany(const Qt::DropActions action)
{
	io::Message bits = Message::None;
	if (action & Qt::CopyAction) {
		bits |= Message::Copy;
	}
	if (action & Qt::MoveAction) {
		bits |= Message::Move;/// | Message::AtomicMove;
	}
	if (action & Qt::LinkAction) {
		mtl_trace();
	}
	return bits;
}

const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;

struct FilesData {
	static const u16 ShowHiddenFiles = 1u << 0;
	static const u16 ThreadMustExit = 1u << 1;
	static const u16 ThreadExited = 1u << 2;
	
	FilesData() {}
	~FilesData();
	FilesData(const FilesData &rhs) = delete;
	std::chrono::time_point<std::chrono::steady_clock> start_time;
	QVector<io::File*> vec;
	/* When needing to select a file sometimes the file isn't yet in
	  the table_model's list because the inotify event didn't tell
	  it yet that a new file is available.
	 i16 holds the count how many times a given filename wasn't found
	 in the current list of files. When it happens a certain amount of
	 times the filename should be deleted from the hash - which is a
	 way to not allow the hash to grow uncontrollably by keeping
	 garbage.
	*/
	i32 skip_dir_id = -1; // to not start selection prematurely
	QHash<QString, i16> filenames_to_select;// <filename, counter>
	bool should_skip_selecting() const { return dir_id == skip_dir_id; }
	
	QString processed_dir_path;
	QString unprocessed_dir_path;
	QString scroll_to_and_select;
	SortingOrder sorting_order;
	i32 dir_id = 0;/// for inotify/epoll
	u16 bits_ = 0;
	cornus::Action action = Action::None;
	
	bool show_hidden_files() const { return bits_ & ShowHiddenFiles; }
	void show_hidden_files(const bool flag) {
		if (flag)
			bits_ |= ShowHiddenFiles;
		else
			bits_ &= ~ShowHiddenFiles;
	}
	
	bool thread_must_exit() const { return bits_ & ThreadMustExit; }
	void thread_must_exit(const bool flag) {
		if (flag)
			bits_ |= ThreadMustExit;
		else
			bits_ &= ~ThreadMustExit;
	}
	
	bool thread_exited() const { return bits_ & ThreadExited; }
	void thread_exited(const bool flag) {
		if (flag)
			bits_ |= ThreadExited;
		else
			bits_ &= ~ThreadExited;
	}
};

struct Files {
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	FilesData data = {};
	
	inline void Broadcast() {
		int status = pthread_cond_broadcast(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	MutexGuard guard() const {
		return MutexGuard(&mutex);
	}
	
	inline int Lock() {
		int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
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
	
	inline int CondWait() {
		return pthread_cond_wait(&cond, &mutex);
	}
};

struct CountRecursiveInfo {
	i64 size = 0;
	i64 trash_size = 0;
	
	i32 file_count = 0;
	i32 trash_file_count = 0;
	
	i32 dir_count = -1; // -1 to exclude counting parent folder
	i32 trash_dir_count = 0;
};
}
Q_DECLARE_METATYPE(cornus::io::FilesData*);
Q_DECLARE_METATYPE(cornus::io::CountRecursiveInfo*);

namespace cornus::io {

int CompareStrings(const QString &a, const QString &b);

// returns a negative number on error
int CountDirFilesSkippingSubdirs(const QString &dir_path);

bool CopyFileFromTo(const QString &from_full_path, QString to_dir);

struct CountFolderData {
	io::CountRecursiveInfo info = {};
	QString full_path;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	bool thread_has_quit = false;
	bool app_has_quit = false;
	struct statx stx;
	QString err_str;
	
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
	
	MutexGuard guard() { return MutexGuard(&mutex); }
};

bool CountSizeRecursive(const QString &path, struct statx &stx,
	CountRecursiveInfo &info);

// returns errno, or zero for success
int CountSizeRecursiveTh(const QString &path, CountFolderData &data, const bool inside_trash);

media::ShortData* DecodeShort(ByteArray &ba);

void Delete(io::File *file);

int DeleteFolder(QString dir_path); // returns 0 on success

bool DirExists(const QString &full_path);

int DoStat(const QString &full_path, const QString &name,
	bool &is_trash_dir, const bool do_check, CountFolderData &data);

bool EnsureDir(QString dir_path, const QString &subdir, QString *result = nullptr);

bool EnsureRegularFile(const QString &full_path);

enum class AppendSlash: i8 {
	Yes,
	No,
};

bool ExpandLinksInDirPath(QString &unprocessed_dir_path,
	QString &processed_dir_path, const AppendSlash afs = AppendSlash::Yes);

bool FileContentsContains(const QString &full_path,
	const QString &searched_str);

bool FileExistsCstr(const char *path, FileType *file_type = nullptr);

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

QStringRef
GetParentDirPath(const QString &full_path);

Bool HasExecBit(const QString &full_path);

void InitEnvInfo(Category &desktop, QVector<QString> &search_icons_dirs,
	QVector<QString> &xdg_data_dirs,
	QHash<QString, Category> &possible_categories);

inline bool IsNearlyEqual(double x, double y);

/// lists only dir names, returns 0 on success, errno otherwise
int ListDirNames(QString dir_path, QVector<QString> &vec,
	const ListDirOption option = ListDirOption::IncludeLinksToDirs);

int /// lists files and folders, returns 0 on success
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

void PasteLinks(const QVector<QString> &full_paths, QVector<QString> &filenames, QString target_dir, QString *err);
void PasteRelativeLinks(const QVector<QString> &full_paths,
	QVector<QString> &filenames, QString target_dir, QString *err);

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
	const PostWrite post_write = PostWrite::DoNothing,
	mode_t *custom_mode = nullptr);

} // cornus::io::::
