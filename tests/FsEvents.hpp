#pragma once

#include <QTest>

class FsEvents: public QObject
{
	Q_OBJECT
	
public:
	QString PrepareFolder(QStringView subdir);
private Q_SLOTS:
	void newFiles();
};

#include "FsEvents.moc"
