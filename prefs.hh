#pragma once

#include <QString>
#include "types.hxx"

namespace cornus::prefs {
const QString AppConfigName = QLatin1String("CornusMas");
const QString BookmarksFileName = QLatin1String("bookmarks.dat");
const u16 BookmarksFormatVersion = 1;

QString QueryAppConfigPath();
}
