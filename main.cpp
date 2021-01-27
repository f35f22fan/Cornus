#include "App.hpp"

#include <cstdlib>

#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[]) {
	QApplication qapp(argc, argv);
	
	cornus::App app;
	app.show();
	
	return qapp.exec();
}
