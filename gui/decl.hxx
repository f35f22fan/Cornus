#pragma once

#include "../types.hxx"

namespace cornus::gui {
class Location;
class Table;
class TableDelegate;
class TableModel;
class ToolBar;


enum class Column : i8 {
	Icon = 0,
	FileName,
	Size,
	Count
};
}
