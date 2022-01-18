#include "SaveFile.hpp"

#include "io.hh"

namespace cornus::io {

SaveFile::SaveFile(const QString &dir_path, const QString &filename)
{
	original_path_ = dir_path;
	if (!original_path_.endsWith('/'))
		original_path_.append('/');
	original_path_.append(filename);
}

SaveFile::~SaveFile()
{
	if (!committed_) {
		mtl_warn("You forgot to commit()");
	}
}

bool SaveFile::Commit(QString *ret_err_str)
{
	committed_ = true;
	
	if (temp_path_.isEmpty() && !InitTempPath())
	{
		if (ret_err_str)
			*ret_err_str = QLatin1String("InitTempPath() failed");
		return false;
	}
	
	auto new_ba = original_path_.toLocal8Bit();
	auto old_ba = temp_path_.toLocal8Bit();
	
	if (::rename(old_ba.data(), new_ba.data()) == 0)
		return true;
	
	if (ret_err_str)
		*ret_err_str = strerror(errno);
	
	return false;
}

const QString &SaveFile::GetPathToWorkWith()
{
	if (temp_path_.isEmpty())
		InitTempPath();
	
	return temp_path_;
}

bool SaveFile::InitTempPath()
{
	temp_path_ = original_path_ + QLatin1String(".tmp_cornus_mas");
	
	return io::EnsureRegularFile(temp_path_);
}

}
