#pragma once

#include "../gui/decl.hxx"
#include "../err.hpp"
#include "io.hh"

#include <QIcon>
#include <QMimeDatabase>

namespace cornus::io {

struct FileCache {
	QString mime;
	QStringRef ext = QStringRef();
	QIcon *icon = nullptr;
};

class File {
public:
	File(Files *files);
	File(const QString &dir_path);
	virtual ~File();
	
	static File* NewTextFile(const QString &dir_path, const QString &name);
	static File* NewFolder(const QString &dir_path, const QString &name);
	bool DeleteFromDisk();
	
	File* Clone() const;
	
	QString build_full_path() const;
	FileCache& cache() { return cache_; }
	void ClearCache();
	bool has_exec_bit() const;
	
	bool is_dir() const { return type_ == FileType::Dir; }
	bool is_link_to_dir() const { return is_symlink() &&
		(link_target_ != nullptr) &&
		(link_target_->type == FileType::Dir); }
	bool is_dir_or_so() const { return is_dir() || is_link_to_dir(); }
	bool is_regular() const { return type_ == FileType::Regular; }
	bool is_symlink() const { return type_ == FileType::Symlink; }
	bool is_pipe() const { return type_ == FileType::Pipe; }
	bool is_socket() const { return type_ == FileType::Socket; }
	bool is_bloc_device() const { return type_ == FileType::BlockDevice; }
	bool is_char_device() const { return type_ == FileType::CharDevice; }
	
	bool selected() const { return bits_ & FileBits::Selected; }
	void selected(const bool flag) {
		if (flag)
			bits_ |= FileBits::Selected;
		else
			bits_ &= ~FileBits::Selected;
	}
	
	LinkTarget *link_target() const { return link_target_; }
	void mode(mode_t m) { mode_ = m; }
	mode_t mode() const { return mode_; }
	const QString& name() const { return name_.orig; }
	const QString& name_lower() const { return name_.lower; }
	void name(const QString &s);
	
	void dir_path(const QString &s) { dp_ = s; }
	
	const QString& dir_path() const {
		return (files_ == nullptr) ? dp_ : files_->dir_path; }
	
	io::Files* files() const { return files_; }
	
	void id(const FileID d) { id_ = d; }
	FileID id() const { return id_; }
	
	QString SizeToString() const;
	
	i64 size() const { return size_; }
	void size(const i64 n) { size_ = n; }
	
	const struct statx_timestamp&
	time_created() const { return time_created_; }
	void time_created(const struct statx_timestamp &t) { time_created_ = t; }
	
	const struct statx_timestamp&
	time_modified() const { return time_modified_; }
	void time_modified(const struct statx_timestamp &t) { time_modified_ = t; }
	
	void type(const FileType t) { type_ = t; }
	FileType type() const { return type_; }
	
private:
	NO_ASSIGN_COPY_MOVE(File);
	
	void ReadExtension();
	
	struct Name {
		QString orig;
		QString lower;
	};
	
	LinkTarget *link_target_ = nullptr;
	Name name_;
	FileCache cache_ = {};
	io::Files *files_ = nullptr;
	QString dp_;
	i64 size_ = -1;
	FileID id_ = {};
	struct statx_timestamp time_created_ = {};
	struct statx_timestamp time_modified_ = {};
	mode_t mode_;
	u8 bits_ = 0;
	FileType type_ = FileType::Unknown;
	
	friend io::Err io::ListFiles(io::Files &files, FilterFunc ff);
	friend class cornus::gui::TableModel;
};

}
