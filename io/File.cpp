#include "File.hpp"

#include "../ByteArray.hpp"
#include "Files.hpp"
#include "../thumbnail.hh"

#include <QImageReader>

#include <cstdio>
#include <zstd.h>

namespace cornus::io {

File::File(Files *files): files_(files) {}
File::File(const QString &dir_path): dp_(dir_path) {}
File::~File()
{
	if (link_target_ != nullptr) {
		delete link_target_;
		link_target_ = nullptr;
	}
	
	delete cache_.thumbnail;
	cache_.thumbnail = nullptr;
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
	file->cache_.thumbnail = nullptr;
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

void File::CountDirFiles1Level()
{
	if (!is_dir_or_so())
		return;
	
	QByteArray ba;
	if (is_dir())
	{
		ba = build_full_path().toLocal8Bit();
	} else {
		ba = link_target_->path.toLocal8Bit();
	}
	
	//mtl_info("%s", ba.data());
	DIR *dp = opendir(ba.data());
	
	if (dp == NULL)
	{
		dir_file_count_1_level_ = -1;
		return;// errno;
	}
	
	int n = 0;
	while (readdir(dp))
	{
		n++;
	}
	
	closedir(dp);
	dir_file_count_1_level_ = std::max(0, n - 2);
}

int File::Delete() {
	auto ba = build_full_path().toLocal8Bit();
	return remove(ba.data());
}

const QString& File::dir_path() const
{
	return (files_ == nullptr) ? dp_ : files_->data.processed_dir_path;
}

bool File::has_exec_bit() const {
	if (has_link_target()) {
		return link_target_->mode & io::ExecBits;
	}
	return mode_ & io::ExecBits;
}

bool File::IsThumbnailMarkedFailed()
{
	ByteArray &ba = ext_attrs_[media::XAttrThumbnail];
	return ba.size() <= (ThumbnailHeaderSize + 4);
}

void File::MarkThumbnailFailed()
{
	ByteArray ba;
	ba.add_i32(-1);
	ext_attrs_.insert(media::XAttrThumbnail, ba);
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
	// Must be lower case because other things like image type
	// detection rely on a lower case filename extension.
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

bool File::ShouldTryLoadingThumbnail(bool &is_webp)
{
	if (!is_regular() || cache_.tried_loading_thumbnail)
		return false;
	
	static const auto formats = QImageReader::supportedImageFormats();
	is_webp = (cache_.ext == QLatin1String("webp"));
	const auto ext_ba = cache_.ext.toLocal8Bit();
	if (!is_webp && !formats.contains(ext_ba))
		return false;
	
	const bool has_attr = has_thumbnail_attr();
	
	if (has_attr && IsThumbnailMarkedFailed())
		return false;
	
	if (cache_.thumbnail == nullptr || has_attr)
		return true;
	
	return false;
}

QString File::SizeToString() const
{
	if (!is_dir_or_so())
		return io::SizeToString(size_);
	
	if (dir_file_count_1_level_ <= 0)
		return QString();
	
	const QString s = QString::number(dir_file_count_1_level_);
	return QChar('(') + s + QChar(')');
}

}
