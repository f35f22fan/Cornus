#pragma once

#include <QMimeData>

#include "types.hxx"

namespace cornus::ui {

enum class DndType: i1 {
	None,
	Urls,
	Ark
};

DndType GetDndType(const QMimeData *md);
}
