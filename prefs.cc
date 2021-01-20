#include "prefs.hh"

#include <QString>
#include <QStandardPaths>

#include "io/io.hh"

namespace cornus::prefs {

QString QueryAppConfigPath()
{
	static QString dir_path = QString();
	
	if (!dir_path.isEmpty())
		return dir_path;
	
	QString config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
	
	if (!config_path.endsWith('/'))
		config_path.append('/');
	
	if (io::EnsureDir(config_path, prefs::AppConfigName))
		return config_path + prefs::AppConfigName;
	
	return QString();
}
}