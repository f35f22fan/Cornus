#pragma once

#include <QString>

namespace cornus::str {
static const QString NautilusClipboardMime = QLatin1String("x-special/nautilus-clipboard");
static const QString GnomeCopiedFiles = QLatin1String("x-special/gnome-copied-files");
static const QString KdeCutMime = QStringLiteral("application/x-kde-cutselection");
static const QString Desktop = QStringLiteral("desktop");
static const QString MediaDirName = QStringLiteral("Media");

namespace root {
static const QString ArgCopyPaste = QStringLiteral("CopyPaste");
static const QString ArgCutPaste = QStringLiteral("CutPaste");
static const QString ArgDelete = QStringLiteral("Delete");
static const QString ArgRename = QStringLiteral("Rename");
}

}
