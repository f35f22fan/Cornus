#include "File.hpp"

#include <cstdio>

namespace cornus::io {

File::File(Files *files): files_(files) {}
File::File(const QString &dir_path): dp_(dir_path) {}
File::~File() {
	if (link_target_ != nullptr) {
		delete link_target_;
		link_target_ = nullptr;
	}
}

QString
File::build_full_path() const
{
	if (files_ != nullptr)
		return files_->data.dir_path + name_.orig;
	QString s = dp_;
	
	if (!dp_.endsWith('/'))
		s.append('/');
	
	return s + name_.orig;
}

File*
File::Clone() const
{
	File *file = new File(dir_path());
	file->name_ = name_;
	file->size_ = size_;
	file->type_ = type_;
	file->id_ = id_;
	file->cache_ = cache_;
	file->bits_ = bits_;
	file->time_created_ = time_created_;
	file->time_modified_ = time_modified_;
	
	if (link_target_ != nullptr)
		file->link_target_ = link_target_->Clone();
	
	return file;
}

void
File::ClearCache() {
	cache_.icon = nullptr;
	cache_.mime.clear();
}

bool
File::DeleteFromDisk() {
	auto ba = build_full_path().toLocal8Bit();
	int status = remove(ba.data());
	
	if (status != 0)
		mtl_status(status);
	
	return status == 0;
}

bool
File::has_exec_bit() const {
	const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;
	if (is_symlink()) {
		return link_target_->mode & ExecBits;
	}
	return mode_ & ExecBits;
}

void File::name(const QString &s)
{
	name_.orig = s;
	name_.lower = s.toLower();
	ReadExtension();
}

File*
File::NewTextFile(const QString &dir_path, const QString &name)
{
	auto *file = new File(dir_path);
	file->name(name);
	file->ReadExtension();
	file->type_ = FileType::Regular;
	return file;
}

File*
File::NewFolder(const QString &dir_path, const QString &name)
{
	auto *file = new File(dir_path);
	file->name(name);
	file->type_ = FileType::Dir;
	return file;
}

void File::ReadExtension()
{
	const auto &str = name_.lower;
	int index = str.lastIndexOf('.');
	
	if (index >= 0 && index < (str.size() - 1))
		cache_.ext = str.midRef(index + 1);
	else
		cache_.ext = {};
}

QString
File::SizeToString() const
{
	if (is_dir_or_so())
		return QString();
	
	return io::SizeToString(size_);
}

}
