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
#include <zstd.h>

#include <QHash>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QMimeData>
#include <QProcessEnvironment>
#include <QtCore5Compat/QStringRef>
#include <QVector>
#include <QUrl>

namespace cornus::io {

struct ReadParams {
	i64 read_max = -1;
	mode_t *ret_mode = nullptr;
	PrintErrors print_errors = PrintErrors::Yes;
	CanRelyOnStatxSize can_rely = CanRelyOnStatxSize::No;
};

struct SaveThumbnail {
	QString full_path;
	QImage thmb;
	i32 orig_img_w = -1;
	i32 orig_img_h = -1;
	TempDir dir;
	DiskFileId id;
};

enum class FileEventType: u8 {
	None = 0,
	Modified,
	Deleted,
	Created,
	Renamed,
};

struct FileEvent {
	io::File *new_file = nullptr;
	QString from_name;
	QString to_name;
	int dir_id = -1;
	int index = -1;
	int renaming_deleted_file_at = -1;
	FileEventType type = FileEventType::None;
};

enum class PostWrite: i8 {
	DoNothing = 0,
	FSync,
	FDataSync
};

using MessageType = u32;
cint MessageBitsStartAt = 28;
const MessageType MessageBitsMask = 15;
enum class Message: MessageType {
	Empty = 0,
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
	PasteLinks,
	PasteRelativeLinks,
	RenameFile,
	
	Pasted_Hint = 1u << 28,
	Copy = 1u << 29, // copies files
	DontTryAtomicMove = 1u << 30, // moves with rename()
	Move = 1u << 31, // moves by copying to new dir and deleting old ones
};

inline Message operator | (Message a, Message b) {
	return static_cast<Message>(static_cast<MessageType>(a)
	| static_cast<MessageType>(b));
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
	io::Message bits = Message::Empty;
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
	io::Message bits = Message::Empty;
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

struct CountRecursiveInfo {
	i64 size = 0;
	i64 trash_size = 0;
	
	i32 file_count = 0;
	i32 trash_file_count = 0;
	
	i32 dir_count = 0;
	i32 trash_dir_count = 0;
};
}
Q_DECLARE_METATYPE(cornus::io::CountRecursiveInfo*);

namespace cornus::io {

QString BuildTempPathFromID(const DiskFileId &id);

bool CanWriteToDir(QStringView dir_path);

bool CheckDesktopFileABI(ByteArray &ba);

int CompareStrings(QStringView a, QStringView b);

// returns a negative number on error
int CountDirFilesSkippingSubdirs(QStringView dir_path);

bool CopyFileFromTo(QStringView from_full_path, QString to_dir);

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
	CountRecursiveInfo &info, const FirstTime ft = FirstTime::Yes);

// returns errno, or zero for success
int CountSizeRecursiveTh(const QString &path, CountFolderData &data, const bool inside_trash);

bool CreateRegularFile(QStringView full_path);

// returns -1 on error, fd otherwise
int CreateAutoRenamedFile(QString dir_path, QString filename,
	cint file_flags, const mode_t mode);

media::MediaPreview* CreateMediaPreview(ByteArray &ba);

void Delete(io::File *file, const QProcessEnvironment &env);

int DeleteFolder(QString dir_path, const DeleteSubFolders dsf = DeleteSubFolders::Yes,
	const DeleteTopFolder dtf = DeleteTopFolder::Yes); // returns 0 on success

bool DirExists(const QString &full_path);

int DoStat(const QString &full_path, const QString &name,
	bool &is_trash_dir, const bool do_check, CountFolderData &data);

bool EnsureDir(QString dir_path, const QString &subdir, QString *result = nullptr);

bool EnsureRegularFile(const QString &full_path, const mode_t *mode = nullptr);

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

io::File* FileFromPath(const QString &full_path, int *ret_error = nullptr);

void FillInStx(io::File &file, const struct statx &st, const QString *name);

QString FloatToString(const float number, cint precision);

void GetClipboardFiles(const QMimeData &mime, Clipboard &cl);

DirType GetDirType(const QString &full_path);

QStringView GetFileNameExtension(QStringView name, QStringView *base_name = 0);

QStringView GetFileNameOfFullPath(QStringView full_path);

QString GetLastingTmpDir();

QStringView GetParentDirPath(QStringView full_path);

Bool HasExecBit(const QString &full_path);

void InitEnvInfo(Category &desktop, QVector<QString> &search_icons_dirs,
	QVector<QString> &xdg_data_dirs,
	QHash<QString, Category> &possible_categories, QProcessEnvironment &env);

inline bool IsNearlyEqual(double x, double y);

/// lists only dir names, returns 0 on success, errno otherwise
int ListDirNames(QString dir_path, QVector<QString> &vec,
	const ListDirOption option = ListDirOption::IncludeLinksToDirs);

bool ListFileNames(QStringView full_dir_path, QVector<QString> &vec,
	FilterFunc ff = nullptr);

bool ListFiles(FilesData &data, Files *ptr, const QProcessEnvironment &env, const CountDirFiles cdf,
	const QHash<QString, Category> *possible_categories = nullptr, FilterFunc ff = nullptr);

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

QString MergeList(QStringList list, QChar delim);

QString NewNamePattern(QStringView filename, i32 &next);

inline bool NewThread(void* (*start_routine)(void *), void *arg,
	const PrintErrors pe = PrintErrors::Yes, pthread_t *result = 0)
{
	pthread_t th;
	cint status = pthread_create(&th, NULL, start_routine, arg);
	if (status != 0 && pe == PrintErrors::Yes)
		mtl_status(status);
	
	if (result)
		*result = th;
	
	return (status == 0);
}

void PasteLinks(const QVector<QString> &full_paths,
	QString target_dir, QVector<QString> *filenames, QString *err = nullptr);

void PasteRelativeLinks(const QVector<QString> &full_paths, QString target_dir,
	QVector<QString> *filenames, QString *err = nullptr);

QString PrepareTestingFolder(QStringView subdir);

void ProcessMime(QString &mime);

const char* QuerySocketFor(const QString &dir_path, bool &needs_root);

i64 ReadEventFd(cint fd);

bool ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const ReadParams &params);

bool ReadLink(const char *file_path, LinkTarget &link_target,
	const QString &parent_dir = QString(), const PrintErrors pe = PrintErrors::Yes);

bool ReadLinkSimple(const char *file_path, QString &result);

// returns -1 on error or num read bytes
i64 ReadToBuf(cint fd, char *buf, ci64 buf_size,
	const PrintErrors pe = PrintErrors::No);

bool ReloadMeta(io::File &file, struct statx &stx, const QProcessEnvironment &env,
	const PrintErrors pe, QString *dir_path = nullptr);

void RemoveEFA(const QString &full_path, QVector<QString> names,
	const PrintErrors pe = PrintErrors::No);

// returns 0 on success or errno on failure
int Rename(QStringView old_path, QStringView new_path);

bool SameFiles(const QString &path1, const QString &path2,
	int *ret_error = nullptr);

bool SaveThumbnailToDisk(const SaveThumbnail &item, ZSTD_CCtx *compress_ctx,
	const bool ok_to_store_thumbnails_in_ext_attrs);

bool sd_nvme(const QString &name);

bool valid_dev_path(const QString &name);

QString SizeToString(ci64 sz, const StringLength len = StringLength::Normal);

bool SetEFA(const QString &full_path, const QString &xattr_name,
	const ByteArray &ba, const PrintErrors = PrintErrors::Yes);

bool SortFiles(File *a, File *b);

QString thread_id_short(const pthread_t &th);

isize TryReadFile(const QString &full_path, char *buf,
	ci64 how_much, ExecInfo *info = nullptr);

bool WriteToFile(const QString &full_path, const char *data, ci64 size,
	const PostWrite post_write = PostWrite::DoNothing,
	mode_t *custom_mode = nullptr);

} // cornus::io::::
Q_DECLARE_METATYPE(cornus::io::FileEvent);
