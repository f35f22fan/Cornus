#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "../types.hxx"

#include <chrono>

#include <QHash>
#include <QVector>

namespace cornus::io {

class FilesData {
	static const u16 ShowHiddenFiles = 1u << 0;
	static const u16 ThreadMustExit = 1u << 1;
	static const u16 ThreadExited = 1u << 2;
	static const u16 CanWriteToDir = 1u << 3;
	static const u16 CountDirFiles1Level = 1u << 4;
	static const u16 Reloaded = 1u << 5;
	
public:
	FilesData();
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
	DirId skip_dir_id = -1; // to not start selection prematurely
	QHash<QString, i16> filenames_to_select;// <filename, counter>
	bool should_skip_selecting() const { return dir_id == skip_dir_id; }
	
	QString processed_dir_path;
	QString unprocessed_dir_path;
	QString scroll_to_and_select;
	SortingOrder sorting_order;
	DirId dir_id = 0;/// for inotify/epoll
	int signal_quit_fd = -1;
	u16 bits_ = 0;
	cornus::Action action = Action::None;
	
	bool can_write_to_dir() const { return bits_ & CanWriteToDir; }
	void can_write_to_dir(const bool flag) {
		if (flag)
			bits_ |= CanWriteToDir;
		else
			bits_ &= ~CanWriteToDir;
	}
	
	bool count_dir_files_1_level() const { return bits_ & CountDirFiles1Level; }
	void count_dir_files_1_level(const bool flag) {
		if (flag)
			bits_ |= CountDirFiles1Level;
		else
			bits_ &= ~CountDirFiles1Level;
	}
	
	bool reloaded() const { return bits_ & Reloaded; }
	void reloaded(const bool flag) {
		if (flag)
			bits_ |= Reloaded;
		else
			bits_ &= ~Reloaded;
	}
	
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

class Files {
public:
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	FilesData data = {};
	
	// ==> only used in gui thread
	int cached_files_count = -1;
	bool first_time = true;
	// <== only used in gui thread
	
	// returns cloned file
	io::File* GetFileAtIndex(const cornus::Lock l, const int index);
	int GetFirstSelectedFile(const cornus::Lock l, io::File **ret_cloned_file = nullptr,
		const Clone c = Clone::Yes);
	QString GetFirstSelectedFileFullPath(const cornus::Lock l, QString *ext);
	int GetSelectedFilesCount(const cornus::Lock l, QVector<QString> *extensions = nullptr);
	void GetSelectedFileNames(const cornus::Lock l, QVector<QString> &names, const Path path,
		const StringCase str_case = StringCase::AsIs);
	bool HasSelectedFiles(const cornus::Lock l) const;
	QPair<int, int> ListSelectedFiles(const cornus::Lock l, QList<QUrl> &list);
	
	void RemoveThumbnailsFromSelectedFiles();
	void SelectAllFiles(const cornus::Lock l, const Selected flag, QSet<int> &indices);
	void SelectFilenamesLater(const QVector<QString> &names, const SameDir sd = SameDir::No);
	void SelectFileRange(const cornus::Lock l, const int row1, const int row2, QSet<int> &indices);
	void SetLastWatched(const cornus::Lock l, io::File *file);
	
	void WakeUpInotify(const enum Lock l);
	
	inline void Broadcast() {
		int status = pthread_cond_broadcast(&cond);
		if (status != 0)
			mtl_status(status);
	}
	
	MutexGuard guard(const enum Lock l = Lock::Yes) const {
		return (l == Lock::Yes) ? MutexGuard(&mutex) : MutexGuard();
	}
	
	inline bool Lock() {
		const int status = pthread_mutex_lock(&mutex);
		if (status != 0)
			mtl_status(status);
		return (status == 0);
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

}
Q_DECLARE_METATYPE(cornus::io::FilesData*);
