#pragma once

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
	
	File* Clone() const;
	
	QString build_full_path() const;
	FileCache& cache() { return cache_; }
		
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
	
	LinkTarget *link_target() const { return link_target_; }
	
	const QString& name() const { return name_.orig; }
	const QString& name_lower() const { return name_.lower; }
	void name(const QString &s);
	
	const QString& dir_path() const {
		return (files_ == nullptr) ? dp_ : files_->dir_path; }
	
	void id(const FileID d) { id_ = d; }
	FileID id() const { return id_; }
	
	i64 size() const { return size_; }
	void size(const i64 n) { size_ = n; }
	
	void type(const FileType t) { type_ = t; }
	FileType type() const { return type_; }
	
private:
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
	FileType type_ = FileType::Unknown;
	FileID id_ = {};
	
	friend io::Err io::ListFiles(const QString &full_dir_path, io::Files &files,
		const u8 options, FilterFunc ff);
};

}
