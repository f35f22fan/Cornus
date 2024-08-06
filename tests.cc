#include "tests.hh"

#include <QTimer>

#include "io/io.hh"
#include "App.hpp"
#include "gui/Tab.hpp"

namespace cornus::tests {

cint InotifyFinishMs = 500; // 0.5 seconds

void Print(const char *msg, QList<PathAndMode> test_files)
{
	mtl_info("%s", msg);
	for (auto &next: test_files)
	{
		auto ba = next.path.toLocal8Bit();
		mtl_info("File \"%s\"", ba.data());
	}
}

Test::Test(App *p) : app_(p) {}

void Test::PerformCheckSameFiles()
{
	QList<PathAndMode> files_info = app_->tab()->FetchFilesInfo(WhichFiles::All);
	if (files_info.size() != files_info_.size()) {
		mtl_trace();
		Print("Test files:", files_info_);
		Print("Tab files:", files_info);
		status_ = ENOENT;
		return;
	}
	
	for (const auto &next: files_info) {
		if (!files_info_.contains(next)) {
			mtl_trace();
			Print("Test files:", files_info_);
			Print("Tab files:", files_info);
			status_ = ENOENT;
			return;
		}
	}
	
	status_ = 0;
}

CreateNewFiles::CreateNewFiles(App *app): Test(app)
{
	root_dir_ = cornus::io::PrepareTestingFolder(QString("fs-events"));
	if (!root_dir_.endsWith('/'))
		root_dir_.append('/');
	
	auto *tab = app_->tab();
	QObject::connect(tab, &gui::Tab::SwitchedToNewDir, this, &CreateNewFiles::SwitchedToNewDir);
	app->GoTo(root_dir_);
}

void CreateNewFiles::SwitchedToNewDir(QString unprocessed_dir_path,
	QString processed_dir_path)
{
	if (!unprocessed_dir_path.endsWith('/'))
		unprocessed_dir_path.append('/');
	
	if (unprocessed_dir_path != root_dir_ && processed_dir_path != root_dir_)
		return; // not the dir we're interested in
	
	const QString base_name = QLatin1String("file ");
	const QString ext = QLatin1String(".txt");
	for (int i = 0; i < 3; i++) {
		QString path = processed_dir_path + base_name + QString::number(i) + ext;
		if (!io::CreateRegularFile(path)) {
			status_ = EIO; //  Input/output error (POSIX.1-2001).
			mtl_warn("Couldn't create file");
			return;
		}
		
		QString current_path = path;
		for (int j = 0; j < 6; j++) {
			QString new_path = path + QString::number(j);
			cint status = io::Rename(current_path, new_path);
			if (status != 0) {
				mtl_status(status);
			}
			current_path = new_path;
		}
		
		path = current_path;
		
		// S_IFBLK Block special.
		// S_IFCHR Character special.
		// S_IFIFO FIFO special.
		// S_IFREG Regular.
		// S_IFDIR Directory.
		// S_IFLNK Symbolic link.
		// S_IFSOCK Socket.
		files_info_.append({path, S_IFREG});
	}
	
	QTimer::singleShot(InotifyFinishMs, this, &Test::PerformCheckSameFiles);
}

}
