#pragma once

#include "../err.hpp"
#include "../decl.hxx"

#include <QString>

namespace cornus::io {

class SaveFile {
public:
	SaveFile(const QString &dir_path, const QString  &filename);
	SaveFile(const QString &full_path);
	~SaveFile();
	
	const QString& GetPathToWorkWith();
	
	bool Commit(const cornus::PrintErrors pe = PrintErrors::Yes);
	void CommitCancelled() { commit_cancelled_ = true; }
	
private:
	NO_ASSIGN_COPY_MOVE(SaveFile);
	
	bool InitTempPath();
	
	QString original_path_;
	QString temp_path_;
	bool committed_ = false;
	bool commit_cancelled_ = false;
};

}
