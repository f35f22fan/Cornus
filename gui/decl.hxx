#pragma once

#include "../err.hpp"
#include "../MutexGuard.hpp"

#include <QIcon>
#include <QMap>
#include <QString>
#include <pthread.h>
#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus::io {
class File;
}

namespace cornus::gui {
class AttrsDialog;
class CompleterModel;
class ConfirmDialog;
class CountFolder;
class Hiliter;
class IconView;
class Location;
class MediaDialog;
class OpenOrderModel;
class OpenOrderPane;
class OpenOrderTable;
class PrefsPane;
class RestorePainter;
class SearchLineEdit;
class SearchPane;
class Tab;
class TabBar;
class Table;
class TableDelegate;
class TableHeader;
class TableModel;
class TabsWidget;
class TaskGui;
class TasksWin;
class TextEdit;
class TextField;
class ToolBar;
class TreeItem;
class TreeModel;
class TreeView;

const int FileNameRelax = 2;

struct ShiftSelect {
	int base_row = -1;
	int head_row = -1;
};

enum class ScrollBy: i8 {
	LineStep,
	PageStep,
};

enum class ViewMode: i8 {
	None,
	Details,
	Icons,
};

struct InputDialogParams {
	QSize size = {-1, -1};
	i32 selection_start = -1;
	i32 selection_end = -1;
	QString title;
	QString msg;
	QString initial_value;
	QString placeholder_text;
	QIcon *icon = nullptr;
};

enum class HiliteMode: i16 {
	None = -1,
/// First viable mode must start at zero because these are
/// also used as indices into a vector:
	C_CPP = 0,
	PlainText,
	Python,
	ShellScript,
	DesktopFile,
	Assembly_NASM,
	Count
};

enum class Column : i8 {
	Icon = 0,
	FileName,
	Size,
	TimeCreated,
	TimeModified,
	Count,
};

struct InsertArgs {
	QVector<gui::TreeItem*> bookmarks;
	QVector<gui::TreeItem*> partitions;
};

} /// namespace
