#pragma once

#include <QTest>

#include "decl.hxx"

namespace cornus::tests {

class Test: public QObject {
public:
	Test(App*);
	virtual void PerformCheckSameFiles();
	
	inline int status() { return status_; }
	
	int status_ = 0;
	App *app_ = 0;
	QList<PathAndMode> files_info_;
	QString root_dir_; // If initialized must end with '/'
};


class CreateNewFiles: public Test {
	Q_OBJECT
public:
	CreateNewFiles(App *app);
	
public Q_SLOTS:
	void SwitchedToNewDir(QString unprocessed_dir_path, QString processed_dir_path);
};

} // namespace
