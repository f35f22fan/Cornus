#include "DirStream.hpp"

#include "io.hh"

#include <algorithm>
#include <cmath>
#include <bits/stdc++.h> /// std::sort()
#include <sys/xattr.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace cornus::io {

bool DirItem::ReadLinkTarget(QString &result, const Expand expand)
{
	auto ba = (stream->dir_path() + name).toLocal8Bit();
	QString target;
	if (!io::ReadLinkSimple(ba.data(), target))
		return false;
	
	result = stream->dir_path() + target;
	if (expand == Expand::Yes) {
		QString s = result;
		CHECK_TRUE(io::ExpandLinksInDirPath(s, result, AppendSlash::No));
	}
	return true;
}

DirStream::DirStream(QString dir_path, const Expand expand)
{
	dir_item_.stream = this;
	
	if (expand == Expand::Yes) {
		CHECK_TRUE_VOID(io::ExpandLinksInDirPath(dir_path, dir_path_));
	} else {
		dir_path_ = dir_path;
	}
	
	if (!dir_path_.endsWith('/'))
		dir_path_.append('/');
	
	auto ba = dir_path_.toLocal8Bit();
	dir_p_ = opendir(ba.data());
	
	if (!dir_p_) {
		mtl_warn("%s: %s", ba.data(), strerror(errno));
	}
}

DirStream::~DirStream()
{
	if (dir_p_)
		closedir(dir_p_);
}

DirItem*
DirStream::next()
{
	if (!dir_p_)
		return nullptr;
	
	struct dirent *entry = readdir(dir_p_);
	if (!entry)
		return nullptr;
	
	const char *n = entry->d_name;
	if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
		return next();
	
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_ALL;
	dir_item_.name = n;
	auto ba = (dir_path_ + dir_item_.name).toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &dir_item_.stx) != 0) {
		mtl_warn("statx(): %s: \"%s\"", strerror(errno), n);
		return nullptr;
	}
	
	return &dir_item_;
	
//	file.mode(stx.stx_mode);
//	file.size(stx.stx_size);
//	file.type(MapPosixTypeToLocal(stx.stx_mode));
//	file.id(io::FileID::NewStx(stx));
//	file.time_created(stx.stx_btime);
//	file.time_modified(stx.stx_mtime);
}

}


