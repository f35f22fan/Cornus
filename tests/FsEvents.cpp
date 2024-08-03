#include "FsEvents.hpp"

#include "../io/io.hh"

void FsEvents::newFiles()
{
	// QString base_dir = PrepareFolder(QString("events"));
	// if (base_dir.isEmpty())
	// 	return;
	
	mtl_info("executing newFiles()");
}

QString FsEvents::PrepareFolder(QStringView subdir)
{
	QString result;
	if (!cornus::io::EnsureDir(QDir::homePath(), subdir.toString(), &result)) {
		mtl_trace();
		return QString();
	}
	
	auto ba = result.toLocal8Bit();
	mtl_info("the dir: \"%s\"\n", ba.data());
	
	return result;
}

