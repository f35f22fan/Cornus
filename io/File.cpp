#include "File.hpp"

#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
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
		link_target_ = 0;
	}
	
	ClearThumbnail(AlsoDeleteFromDisk::No);
	
	delete cache_.desktop_file;
	cache_.desktop_file = 0;
	
	delete cache_.media_preview;
	cache_.media_preview = 0;
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
	DeleteMediaPreview();
}

void File::ClearThumbnail(AlsoDeleteFromDisk d) {
	delete cache_.thumbnail;
	cache_.thumbnail = 0;
	ext_attrs_.remove(media::XAttrThumbnail);
	
	if (d == AlsoDeleteFromDisk::Yes) {
		QVector<QString> names = {media::XAttrThumbnail};
		io::RemoveEFA(build_full_path(), names);
	}
}

File* File::Clone(const CloneFileOption opt) const
{
	File *file = new File(dir_path(Lock::No));
	file->files_ = files_;
	file->name_ = name_;
	file->size_ = size_;
	file->mode_ = mode_;
	file->ext_attrs_ = ext_attrs_;
	file->type_ = type_;
	file->id_ = id_;
	file->cache_ = cache_;
	
	if (opt & CloneFileOption::NoThumbnail) {
		file->cache_.thumbnail = 0;
	} else {
		if (cache_.thumbnail)
			file->cache_.thumbnail = cache_.thumbnail->Clone();
	}
	file->cache_.media_preview = cache_.media_preview ? cache_.media_preview->Clone() : 0;
	file->cache_.desktop_file = cache_.desktop_file ? cache_.desktop_file->Clone() : 0;
	file->bits_ = bits_;
	file->time_created_ = time_created_;
	file->time_modified_ = time_modified_;
	file->dir_file_count_ = dir_file_count_;
	file->link_target_ = link_target_ ? link_target_->Clone() : nullptr;
	
	return file;
}

void File::ClearCache()
{
	cache_.icon = nullptr;
	cache_.mime.clear();
	
	DeleteMediaPreview();
}

void File::CountDirFiles()
{
	if (!is_dir_or_so() || dir_file_count_ == -2)
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
		dir_file_count_ = -2;
		return;// errno;
	}
	
	int n = 0;
	while (readdir(dp))
	{
		n++;
	}
	
	closedir(dp);
	dir_file_count_ = std::max(0, n - 2);
}

int File::Delete() {
	auto ba = build_full_path().toLocal8Bit();
	return remove(ba.data());
}

void File::DeleteMediaPreview() {
	if (cache_.media_preview != nullptr) {
		delete cache_.media_preview;
		cache_.media_preview = nullptr;
	}
}

const QString& File::dir_path(const Lock l) const
{
	if (files_ == nullptr)
		return dp_;
	
	bool unlock = false;
	if (l == Lock::Yes)
		unlock = files_->TryLock();
	const auto &s = files_->data.processed_dir_path;
	if (unlock)
		files_->Unlock();
	return s;
}

bool File::extensionCanHaveThumbnail() const {
	if (cache_.ext == QLatin1String("webp"))
		return true;
	
	auto extension = cache_.ext.toLocal8Bit();
	static cauto formats = QImageReader::supportedImageFormats();
	
	return formats.contains(extension);
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
	return ba.size() <= (thumbnail::HeaderSize + 4);
}

void File::MarkThumbnailFailed()
{
	ByteArray ba;
	ba.add_i32(-1);
	ext_attrs_.insert(media::XAttrThumbnail, ba);
}

media::MediaPreview*
File::media_attrs_decoded()
{
	if (cache_.media_preview != nullptr)
		return cache_.media_preview;
	
	if (!has_media_attrs()) {
		return nullptr;
	}
	
	cache_.media_preview = io::CreateMediaPreview(media_attrs());
	if (cache_.media_preview == nullptr) {
		ext_attrs_.remove(media::XAttrName);
	}
	
	return cache_.media_preview;
}

void File::name(QStringView s)
{
	name_.orig = s.toString();
	name_.lower = name_.orig.toLower();
	ReadExtension();
}

File* File::NewTextFile(const QString &dir_path, const QString &name)
{
	auto *file = new File(dir_path);
	file->name(name);
	file->ReadExtension();
	file->mode(S_IFREG);
	file->type_ = FileType::Regular;
	return file;
}

File* File::NewFolder(const QString &dir_path, const QString &name)
{
	auto *file = new File(dir_path);
	file->name(name);
	file->mode(S_IFDIR);
	file->type_ = FileType::Dir;
	return file;
}

void File::ReadExtension()
{
	// Must be lower case because other things like image type
	// detection rely on a lower case filename extension.
	QStringView str = name_.lower;
	cint index = str.lastIndexOf('.');
	
	if (index >= 0 && index < (str.size() - 1))
		cache_.ext = str.toString().mid(index + 1);
	else
		cache_.ext = {};
}

void File::ReadLinkTarget(const Lock l)
{
	if (!is_symlink())
		return;
	auto *target = new LinkTarget();
	auto ba = build_full_path().toLocal8Bit();
	io::ReadLink(ba.data(), *target, dir_path(l));
	if (link_target_ != nullptr)
		delete link_target_;
	link_target_ = target;
}

QString File::SizeToString() const
{
	if (!is_dir_or_so())
		return io::SizeToString(size_);
	
	if (dir_file_count_ <= 0)
		return QString();
	
	const QString s = QString::number(dir_file_count_);
	return QChar('(') + s + QChar(')');
}

void File::WatchProp(Op op, cu64 prop)
{
	// const u64 LastWatched = 1;
	// const u64 Watched = 1 << 1;
	cu64 old_props = watch_props();
	if (op == Op::Invert)
		op = (old_props & prop) ? Op::Remove : Op::Add;
	
//	mtl_info("%s prop %lu", (op == Op::Add) ? "Add" : "Remove", prop);
	if (op == Op::Add) {
		if (old_props & prop) {
			return; // already exists
		}
		
		cu64 flags = old_props | prop;
//		mtl_info("flags: %lu", flags);
		ByteArray ba(flags);
		io::SetEFA(build_full_path(), media::WatchProps::Name, ba);
	} else {
		if ((old_props & prop) == 0)
			return; // nothing to remove
		
		u64 without = old_props & ~prop;
		cauto full_path = build_full_path();
		if (without) {
			ByteArray ba;
			ba.add_u64(without);
//			mtl_info("without: %lu, old props: %lu", without, old_props);
			io::SetEFA(full_path, media::WatchProps::Name, ba);
		} else {
			io::RemoveEFA(full_path, {media::WatchProps::Name});
		}
	}
}

}
