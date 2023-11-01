#include "prefs.hh"
#include "str.hxx"

#include <QStandardPaths>

#include "io/io.hh"

namespace cornus::prefs {

QString GetBookmarksFileName() {
	static const QString s = prefs::BookmarksFileName
		+ QString::number(prefs::BookmarksFormatVersion);
	return s;
}

QString GetBookmarksFilePath()
{
	return prefs::QueryAppConfigPath() + '/' + GetBookmarksFileName();
}

QString GetMediaFilePath() {
	QString dir = QueryAppConfigPath();
	if (!dir.endsWith('/'))
		dir.append('/');
	return dir + str::MediaDirName;
}

QString QueryAppConfigPath()
{
	static QString dir_path = QString();
	
	if (!dir_path.isEmpty())
		return dir_path;
	
	QString config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
	
	if (!config_path.endsWith('/'))
		config_path.append('/');
	
	if (!io::EnsureDir(config_path, prefs::AppConfigName))
		return QString();
	
	dir_path = config_path + prefs::AppConfigName;
	return dir_path;
}

QString QueryMimeConfigDirPath()
{
	static QString dir_path = QString();
	
	if (!dir_path.isEmpty())
		return dir_path;
	
	QString config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
	
	if (!config_path.endsWith('/'))
		config_path.append('/');
	
	if (!io::EnsureDir(config_path, prefs::MimeConfigDir)) {
		return QString();
	}
	
	dir_path = config_path + prefs::MimeConfigDir;
	return dir_path;
}
}
