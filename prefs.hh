#pragma once

#include <QString>
#include "types.hxx"

namespace cornus::prefs {
const QString AppConfigName = QLatin1String("CornusMas");
const QString BookmarksFileName = QLatin1String("bookmarks_");
const QString MimeConfigDir = QLatin1String("mimetypes-open-order");
const QString PrefsFileName = QLatin1String("prefs_");
const u16 BookmarksFormatVersion = 1;
const u16 PrefsFormatVersion = 3;

QString QueryAppConfigPath();
QString QueryMimeConfigDirPath();
}
