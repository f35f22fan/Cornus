#pragma once

#include <QSocketNotifier>

namespace cornus {
class MyDaemon: public QObject {
	Q_OBJECT
	
public:
	MyDaemon(QObject *parent = 0);
	~MyDaemon();
	
	// Unix signal handlers.
	static void hupSignalHandler(int unused);
	static void termSignalHandler(int unused);
	
public Q_SLOTS:
	// Qt signal handlers.
	void handleSigHup();
	void handleSigTerm();
	
private:
	QSocketNotifier *snHup;
	QSocketNotifier *snTerm;
};
}
