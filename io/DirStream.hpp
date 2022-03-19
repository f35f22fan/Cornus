#pragma once

#include "../err.hpp"

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

#include <QString>

namespace cornus::io {
class DirStream;
enum class Expand: i1 {
	Yes,
	No,
};

struct DirItem {
	struct statx stx;
	const char *name = nullptr;
	DirStream *stream = nullptr;
	
	bool is_symlink() const { return S_ISLNK(stx.stx_mode); }
	bool ReadLinkTarget(QString &result, const Expand expand);
};

class DirStream {
public:
	DirStream(QString dir_path, const Expand expand = Expand::No);
	virtual ~DirStream();
	
	DirItem* next();
	const QString& dir_path() const { return dir_path_; }

private:
	DirItem dir_item_ = {};
	DIR *dir_p_ = nullptr;
	QString dir_path_;
};
}
