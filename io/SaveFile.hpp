#pragma once

#include "../err.hpp"

#include <QString>

namespace cornus::io {

class SaveFile {
public:
	SaveFile(const QString &dir_path, const QString  &filename);
	~SaveFile();
	
	const QString& GetPathToWorkWith();
	
	bool Commit(QString *ret_err_str = nullptr);
	
private:
	NO_ASSIGN_COPY_MOVE(SaveFile);
	
	bool InitTempPath();
	
	QString original_path_;
	QString temp_path_;
	QString err_str_;
	
	bool committed_ = false;
};

}
