#include "File.hpp"

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
		return files_->dir_path + name_.orig;
	
	if (dp_.endsWith('/'))
		return dp_ + name_.orig;
	return dp_ + '/' + name_.orig;
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
	
	if (link_target_ != nullptr)
		file->link_target_ = link_target_->Clone();
	
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

void File::name(const QString &s)
{
	name_.orig = s;
	name_.lower = s.toLower();
	ReadExtension();
}

}
