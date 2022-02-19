#include "SaveFile.hpp"

#include "io.hh"

#include <fcntl.h>
#include <sys/stat.h>

namespace cornus::io {

SaveFile::SaveFile(const QString &dir_path, const QString &filename)
{
	original_path_ = dir_path;
	if (!original_path_.endsWith('/'))
		original_path_.append('/');
	original_path_.append(filename);
}

SaveFile::SaveFile(const QString &full_path) :
original_path_(full_path)
{}

SaveFile::~SaveFile()
{
	if (!commit_cancelled_ && !committed_) {
		mtl_warn("You forgot to commit(): %s", qPrintable(original_path_));
	}
}

bool SaveFile::Commit(const PrintErrors pe)
{
	committed_ = true;
	
	if (temp_path_.isEmpty() && !InitTempPath())
	{
		if (pe == PrintErrors::Yes)
			mtl_warn("InitTempPath() failed");
		return false;
	}
	
	auto new_ba = original_path_.toLocal8Bit();
	auto old_ba = temp_path_.toLocal8Bit();
	
	if (::rename(old_ba.data(), new_ba.data()) == 0)
		return true;
	
	if (pe == PrintErrors::Yes)
		mtl_status(errno);
	
	return false;
}

const QString& SaveFile::GetPathToWorkWith()
{
	if (!InitTempPath())
		temp_path_.clear();
	
	return temp_path_;
}

bool SaveFile::InitTempPath()
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	auto source_ba = original_path_.toLocal8Bit();
	if (statx(0, source_ba.data(), flags, fields, &stx) != 0)
	{
		mtl_status(errno);
		return false;
	}
	
	temp_path_ = original_path_ + QLatin1String(".tmp_cornus_mas");
	const mode_t mode = stx.stx_mode;
	
	return io::EnsureRegularFile(temp_path_, &mode);
}

}
