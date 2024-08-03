#pragma once

#include <QMap>
#include <QString>

#include "decl.hxx"

namespace cornus::trash {

cint NumberBase = 36;

struct Names {
	QString decoded;
	QString encoded;
};

static const QString XAttrDeleteKey = QStringLiteral("user.CornusMas.d");

bool AddTrashNameToGitignore(const QString &new_path);

const QString& basename();
const QString& basename_regex();

QString CreateGlobalGitignore();

void EmptyRecursively(const QString &dir_path);

QString EnsureTrashForFile(const QString &file_path);

const QString& gitignore_global_path(const QString *override_data = nullptr);

bool ListItems(QStringView dir_path, QMap<i64, QVector<Names>> &hash);

const QString& name();

QString ReadGitignoreGlobal();

inline QString time_to_str(const i64 t) {
	return QString::number(t, NumberBase);
}

}
