#include "App.hpp"
#include "gui/Tab.hpp"

#include <cstdlib>
#include <iostream>
#include <QApplication>
#include <QWidget>

#include "tests.hh"

int main(int argc, char *argv[])
{
	QApplication qapp(argc, argv);
	
	// QString s = "Jos\u{65}\u{301}";
	// mtl_info("String count: %lld (%s)", s.size(), qPrintable(s));
	// s = QString::fromUtf8(u8"abc\U00010139def\U00010102g");
	// mtl_info("Size: %lld, \"%s\"", s.size(), qPrintable(s));
	
	cornus::App app;
	QStringList args = qapp.arguments();
	QList<cornus::tests::Test*> tests;
	
	if (args.size() >= 2 && args[1] == "test") {
		if (args.size() <= 2) {
			mtl_info("Too few args");
			return 1;
		}
		
		if (args[2] == QLatin1String("newFiles")) {
			tests.append(new cornus::tests::CreateNewFiles(&app));
		} else {
			auto ba = args[2].toLocal8Bit();
			mtl_warn("No such test: \"%s\"", ba.data());
		}
		
	}
	
	app.show();
	cint app_status = qapp.exec();
	if (app_status != 0) {
		mtl_status(app_status);
	}
	
	int last_error = 0;
	for (cornus::tests::Test *test: tests) {
		if (last_error == 0 && test->status() != 0)
			last_error = test->status();
		delete test;
	}
	
	if (last_error) {
		mtl_status(last_error);
	} else if (!tests.isEmpty()) {
		mtl_info("Tests executed successfully");
	}
	
	tests.clear();
	
	return last_error ? last_error : app_status;
}


