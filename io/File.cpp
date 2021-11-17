#include "File.hpp"

#include "../ByteArray.hpp"

#include <cstdio>

namespace cornus::io {

File::File(Files *files): files_(files) {}
File::File(const QString &dir_path): dp_(dir_path) {}
File::~File()
{
	if (link_target_ != nullptr) {
		delete link_target_;
		link_target_ = nullptr;
	}
	
	if (cache_.short_data != nullptr)
		delete cache_.short_data;
}

QString File::build_full_path() const
{
	if (files_ != nullptr)
		return files_->data.processed_dir_path + name_.orig;
	QString s = dp_;
	
	if (!dp_.endsWith('/'))
		s.append('/');
	
	return s + name_.orig;
}

void File::ClearXAttrs()
{
	ext_attrs_.clear();
	
	if (cache_.short_data != nullptr) {
		delete cache_.short_data;
		cache_.short_data = nullptr;
	}
}

File* File::Clone() const
{
	File *file = new File(dir_path());
	file->name_ = name_;
	file->size_ = size_;
	file->mode_ = mode_;
	file->ext_attrs_ = ext_attrs_;
	file->type_ = type_;
	file->id_ = id_;
	file->cache_ = cache_;
	file->cache_.short_data = nullptr;
	file->bits_ = bits_;
	file->time_created_ = time_created_;
	file->time_modified_ = time_modified_;
	
	if (link_target_ != nullptr)
		file->link_target_ = link_target_->Clone();
	
	return file;
}

void File::ClearCache()
{
	cache_.icon = nullptr;
	cache_.mime.clear();
	
	if (cache_.short_data != nullptr) {
		delete cache_.short_data;
		cache_.short_data = nullptr;
	}
}

bool File::DeleteFromDisk() {
	auto ba = build_full_path().toLocal8Bit();
	int status = remove(ba.data());
	
	if (status != 0) {
		mtl_warn("Failed to delete \"%s\": %s", ba.data(), strerror(errno));
	}
	
	return status == 0;
}

bool File::has_exec_bit() const {
	if (has_link_target()) {
		return link_target_->mode & io::ExecBits;
	}
	return mode_ & io::ExecBits;
}

media::ShortData*
File::media_attrs_decoded()
{
	if (cache_.short_data != nullptr)
		return cache_.short_data;
	
	if (!has_media_attrs())
		return nullptr;
	
	ByteArray ba = media_attrs();
	cache_.short_data = io::DecodeShort(ba);
	if (cache_.short_data == nullptr)
		ext_attrs_.remove(media::XAttrName);
	
	return cache_.short_data;
}

void File::name(const QString &s)
{
	name_.orig = s;
	name_.lower = s.toLower();
	ReadExtension();
}

File* File::NewTextFile(const QString &dir_path, const QString &name)
{
	auto *file = new File(dir_path);
	file->name(name);
	file->ReadExtension();
	file->type_ = FileType::Regular;
	return file;
}

File* File::NewFolder(const QString &dir_path, const QString &name)
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

void File::ReadLinkTarget()
{
	if (!is_symlink())
		return;
	auto *target = new LinkTarget();
	auto ba = build_full_path().toLocal8Bit();
	io::ReadLink(ba.data(), *target, dir_path());
	if (link_target_ != nullptr)
		delete link_target_;
	link_target_ = target;
}

QString File::SizeToString() const
{
	if (is_dir_or_so())
		return QString();
	
	return io::SizeToString(size_);
}

}
