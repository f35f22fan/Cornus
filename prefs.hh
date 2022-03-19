#pragma once

#include <QString>
#include "types.hxx"

namespace cornus::prefs {
const QString AppConfigName = QLatin1String("CornusMas");
const QString BookmarksFileName = QLatin1String("bookmarks_");
const QString MimeConfigDir = QLatin1String("mimetypes-open-order");
const QString PrefsFileName = QLatin1String("prefs_");
const u2 BookmarksFormatVersion = 1;
const u2 PrefsFormatVersion = 3;

QString GetBookmarksFileName();
QString GetBookmarksFilePath();
QString GetMediaFilePath();
QString QueryAppConfigPath();
QString QueryMimeConfigDirPath();
}
