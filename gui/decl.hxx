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
class CompleterModel;
class CountFolder;
class Hiliter;
class Location;
class OpenOrderModel;
class OpenOrderPane;
class OpenOrderTable;
class OpenWithPane;
class PrefsPane;
class SearchLineEdit;
class SearchPane;
class ShallowItem;
class SidePane;
class SidePaneItem;
class SidePaneModel;
class Table;
class TableDelegate;
class TableModel;
class TaskGui;
class TasksWin;
class TextEdit;
class ToolBar;

const int FileNameRelax = 2;

enum class FileEventType: u8 {
	None = 0,
	Changed,
	Deleted,
	Created,
	MovedTo,
};

struct FileEvent {
	cornus::io::File *new_file = nullptr;
	int dir_id = -1;
	int index = -1;
	FileEventType type = FileEventType::None;
	bool is_dir = false;
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

} /// namespace
Q_DECLARE_METATYPE(cornus::gui::FileEvent);
