#include "App.hpp"
#include "gui/Tab.hpp"

#include <cstdlib>

#include <QApplication>
#include <QWidget>

#include "tests/FsEvents.hpp"

int main(int argc, char *argv[]) {
	QApplication qapp(argc, argv);
	cornus::App app;
	
	if ((argc >= 2) && (strncmp("test", argv[1], 4) == 0)) {
		int status = 0;
		
		auto runTest = [&status, argc , argv](QObject* obj) {
			status |= QTest::qExec(obj, argc - 1, &argv[1]);
		};
		
		FsEvents fse;
		QString root_dir = fse.PrepareFolder(QString("events"));
		app.tab()->GoToSimple(root_dir);
		runTest(&fse);
	}
	
	app.show();
	return qapp.exec();
}


