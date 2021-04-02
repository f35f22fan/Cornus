#pragma once

#include "err.hpp"

namespace cornus::media {

static const QString XAttrName = QStringLiteral("user.CornusMas.m");

enum class Action: i8 {
	Insert,
	Append
};

enum class Field: u8 {
	None = 0,
	Actors,
	Writers,
	Directors,
	Genres,
	Subgenres,
	Countries,
	Year, // e.g. 2011
	Year2, // e.g. 2011-2015
};

}
