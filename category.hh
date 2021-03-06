#pragma once

#include "err.hpp"

#include <QHash>

namespace cornus {

enum class Category: u8
{
	None = 0,
	TextEditor,
	Viewer,
	Player,
	Photography,
	AudioVideo,
	Audio,
	Video,
	KDE,
	Gnome,
	Ubuntu,
	Unity,
	Xfce,
	Qt,
	Gtk,
};

namespace category {
void InitAll(QHash<QString, Category> &h);
} /// namespace::

namespace str {
static const QString TextEditor = QStringLiteral("texteditor");
static const QString Photography = QStringLiteral("photography");
static const QString Viewer = QStringLiteral("viewer");
static const QString Player = QStringLiteral("player");
static const QString Audio = QStringLiteral("audio");
static const QString Video = QStringLiteral("video");
static const QString AudioVideo = QStringLiteral("audiovideo");

static const QString Gnome = QStringLiteral("gnome");
static const QString KDE = QStringLiteral("kde");
static const QString Xfce = QStringLiteral("xfce");
static const QString Ubuntu = QStringLiteral("ubuntu");
static const QString Unity = QStringLiteral("unity");
static const QString Gtk = QStringLiteral("gtk");
static const QString Qt = QStringLiteral("qt");
}

}
