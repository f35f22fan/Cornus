#include "Table.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "CountFolder.hpp"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "OpenOrderPane.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../io/socket.hh"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "../str.hxx"
#include "TableDelegate.hpp"
#include "TableModel.hpp"

#include <map>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QUrl>

namespace cornus::gui {

const QVector<QString> ArchiveExtensions = {
	QLatin1String("zip"), QLatin1String("xz"), QLatin1String("gz"),
	QLatin1String("odt"), QLatin1String("ods"), QLatin1String("rar"),
	QLatin1String("7z"), QLatin1String("tar"), QLatin1String("iso"),
	QLatin1String("bz2"), QLatin1String("lz"), QLatin1String("lz4"),
	QLatin1String("lzma"), QLatin1String("lzo"), QLatin1String("z"),
	QLatin1String("jar"), QLatin1String("zst"), QLatin1String("dmg")
};

Table::Table(TableModel *tm, App *app) : app_(app),
model_(tm)
{
	setModel(model_);
/// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true); 
	delegate_ = new TableDelegate(this, app_);
	setAlternatingRowColors(false);
	auto d = static_cast<QAbstractItemDelegate*>(delegate_);
	setItemDelegate(d);
	{
		auto *hz = horizontalHeader();
		hz->setSortIndicatorShown(true);
		hz->setSectionHidden(int(Column::TimeModified), true);
		hz->setSortIndicator(int(Column::FileName), Qt::AscendingOrder);
		connect(hz, &QHeaderView::sortIndicatorChanged, this, &Table::SortingChanged);
		hz->setSectionsMovable(false);
		
		hz->setContextMenuPolicy(Qt::CustomContextMenu);
		
		connect(hz, &QHeaderView::customContextMenuRequested,
			this, &Table::ShowVisibleColumnOptions);
	}
	
	UpdateLineHeight();
	
	connect(verticalHeader(), &QHeaderView::sectionClicked, [=](int index) {
		model_->app()->DisplayFileContents(index);
	});
	{
		setDragEnabled(true);
		setAcceptDrops(true);
		setDragDropOverwriteMode(false);
		setDropIndicatorShown(true);
		setDefaultDropAction(Qt::MoveAction);
	}
	setUpdatesEnabled(true);
	///setShowGrid(false);
	setSelectionMode(QAbstractItemView::NoSelection);//ExtendedSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	
	int sz = GetIconSize();
	setIconSize(QSize(sz, sz));
	SetCustomResizePolicy();
}

Table::~Table() {
	delete model_;
	
	for (auto *next: open_with_.add_vec) {
		delete next;
	}
	
	for (auto *next: open_with_.remove_vec) {
		delete next;
	}
	
	open_with_.add_vec.clear();
	open_with_.remove_vec.clear();
}

void SendURLsClipboard(const QStringList &list, io::socket::MsgBits msg_id) {
	auto ba = new ByteArray();
	ba->set_msg_id(msg_id);
	for (const auto &next: list)
	{
		ba->add_string(next);
	}
	io::socket::SendAsync(ba);
}

void
Table::AddOpenWithMenuTo(QMenu *main_menu, const QString &full_path)
{
	QVector<QAction*> actions = CreateOpenWithList(full_path);
	
	if (actions.isEmpty())
		return;

	auto *open_with_menu = new QMenu(tr("Open &With"));
	
	for (auto &action: actions) {
		open_with_menu->addAction(action);
	}
	
	open_with_menu->addSeparator();
	QAction *action = new QAction(tr("Preferences.."));
	connect(action, &QAction::triggered, [=] {
		OpenOrderPane pane(app_);
	});
	open_with_menu->addAction(action);
	
	main_menu->addMenu(open_with_menu);
}

void
Table::ActionCopy(QVector<int> &indices)
{
	QStringList list;
	if(!CreateMimeWithSelectedFiles(ClipboardAction::Copy, indices, list))
		return;
	
	SendURLsClipboard(list, io::socket::MsgBits::CopyToClipboard);
}

void
Table::ActionCut(QVector<int> &indices) {
	QStringList list;
	if (!CreateMimeWithSelectedFiles(ClipboardAction::Cut, indices, list))
		return;
	
	SendURLsClipboard(list, io::socket::MsgBits::CutToClipboard);
}

void
Table::ActionPaste()
{
	const QClipboard *qclipboard = QApplication::clipboard();
	const QMimeData *mime = qclipboard->mimeData();
	 // mimeData can be 0 according to https://bugs.kde.org/show_bug.cgi?id=335053
	if (!mime) {
		return;
	}
	
//	cornus::Clipboard clipboard;
//	io::GetClipboardFiles(*mime, clipboard);
	
//	if (!clipboard.has_files())
//		return;
	const Clipboard &clipboard = app_->clipboard();
	
	io::socket::MsgBits io_op = io::socket::MsgBits::None;
	if (clipboard.action == ClipboardAction::Copy)
		io_op = io::socket::MsgBits::Copy;
	else
		io_op = io::socket::MsgBits::Move;
	
	auto *ba = new ByteArray();
	ba->set_msg_id(io_op);
	QString to_dir = app_->current_dir();
	ba->add_string(to_dir);
	ba->add_i32(clipboard.file_paths.size());
	
	for (const auto &next: clipboard.file_paths) {
		ba->add_string(next);
	}
	
	io::socket::SendAsync(ba);
}

void
Table::ActionPasteLinks()
{
	const QClipboard *qclipboard = QApplication::clipboard();
	const QMimeData *mime = qclipboard->mimeData();
	 // mimeData can be 0 according to https://bugs.kde.org/show_bug.cgi?id=335053
	if (!mime) {
		return;
	}
	
//	cornus::Clipboard clipboard;
//	io::GetClipboardFiles(*mime, clipboard);
	
//	if (!clipboard.has_files())
//		return;
	
	const Clipboard &clipboard = app_->clipboard();
	
	io::PasteLinks(clipboard.file_paths, app_->current_dir());
}

bool
Table::AnyArchive(const QVector<QString> &extensions) const
{
	for (auto &next: ArchiveExtensions) {
		if (extensions.contains(next))
			return true;
	}
	
	return false;
}

void
Table::ApplyPrefs()
{
	auto &map = app_->prefs().cols_visibility();
	auto *hz = horizontalHeader();
	QMap<i8, bool>::const_iterator i = map.constBegin();
	
	while (i != map.constEnd()) {
		const bool hidden = !i.value();
		hz->setSectionHidden(i.key(), hidden);
		++i;
	}
}

bool
Table::CheckIsOnFileName(io::File *file, const int file_row, const QPoint &pos) const
{
	if (!file->is_dir() || pos.y() < 0)
		return false;
	
	i32 col = columnAt(pos.x());
	if (col != (int)Column::FileName)
		return false;
	
	if (rowAt(pos.y()) != file_row)
		return false;
	
	QFontMetrics fm = fontMetrics();
	const int name_w = fm.boundingRect(file->name()).width();
	const int absolute_name_end = name_w + columnViewportPosition(col);
	
	return (pos.x() < absolute_name_end + FileNameRelax);
}

void
Table::ClearDndAnimation(const QPoint &drop_coord)
{
	// repaint() or update() don't work because
	// the window is not raised when dragging an item
	// on top of the table and the repaint
	// requests are ignored. Repaint using a hack:
	int row = rowAt(drop_coord.y());
	
	if (row != -1) {
		int start = row;
		if (row > 0)
			start--;
		int end = row + 1;
		model_->UpdateRowRange(start, end);
	}
}

bool
Table::CreateMimeWithSelectedFiles(const ClipboardAction action,
	QVector<int> &indices, QStringList &list)
{
	auto &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	io::FileBits flag = io::FileBits::Empty;
	if (action == ClipboardAction::Cut)
		flag = io::FileBits::ActionCut;
	else if (action == ClipboardAction::Copy)
		flag = io::FileBits::ActionCopy;
	else if (action == ClipboardAction::Paste)
		flag = io::FileBits::ActionPaste;
	else if (action == ClipboardAction::Link)
		flag = io::FileBits::PasteLink;
	
	int i = -1;
	for (io::File *next: files.data.vec)
	{
		i++;
		
		if (next->selected()) {
			indices.append(i);
			next->toggle_flag(flag, true);
			QString s = next->build_full_path();
			list.append(QUrl::fromLocalFile(s).toString());
		} else {
			if (next->clear_all_actions_if_needed())
				indices.append(i);
		}
	}
	
	return true;
}

QVector<QAction*>
Table::CreateOpenWithList(const QString &full_path)
{
	for (auto *next: open_with_.add_vec) {
		delete next;
	}
	for (auto *next: open_with_.remove_vec) {
		delete next;
	}
	open_with_.add_vec.clear();
	open_with_.remove_vec.clear();
	open_with_.full_path = full_path;
	
	QString mime = app_->QueryMimeType(full_path);
	QVector<QAction*> ret;
	
	int fd = io::socket::Client(cornus::SocketPath);
	if (fd == -1)
		return ret;
	
	ByteArray send_ba;
	send_ba.set_msg_id(io::socket::MsgBits::SendOpenWithList);
	send_ba.add_string(mime);
	
	if (!send_ba.Send(fd, false))
		return ret;
	
	ByteArray receive_ba;
	if (!receive_ba.Receive(fd))
		return ret;
	
	while (receive_ba.has_more()) {
		const DesktopFile::Action action = (DesktopFile::Action)receive_ba.next_i8();
		auto *next = DesktopFile::From(receive_ba);
		if (next != nullptr) {
			if (action == DesktopFile::Action::Add)
				open_with_.add_vec.append(next);
			else
				open_with_.remove_vec.append(next);
		}
	}
	
	for (DesktopFile *next: open_with_.add_vec)
	{
		QString n = next->GetName();
		QAction *action = new QAction(n);
		QVariant v = QVariant::fromValue((void *) next);
		action->setData(v);
		action->setIcon(next->CreateQIcon());
		connect(action, &QAction::triggered, this, &Table::LaunchFromOpenWithMenu);
		ret.append(action);
	}
	
	return ret;
}

void
Table::dragEnterEvent(QDragEnterEvent *evt)
{
	const QMimeData *mimedata = evt->mimeData();
	
	if (mimedata->hasUrls()) {
		evt->acceptProposedAction();
	}
}

void
Table::dragLeaveEvent(QDragLeaveEvent *evt)
{
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

void
Table::dragMoveEvent(QDragMoveEvent *event)
{
	drop_coord_ = event->pos();
	ClearDndAnimation(drop_coord_);
}

void
Table::dropEvent(QDropEvent *evt)
{
mtl_info("Drop function start");
	if (evt->mimeData()->hasUrls()) {
mtl_info("Drop proceed");
		QVector<io::File*> *files_vec = new QVector<io::File*>();
		
		for (const QUrl &url: evt->mimeData()->urls())
		{
			io::File *file = io::FileFromPath(url.path());
			if (file != nullptr)
				files_vec->append(file);
		}
		
		io::File *to_dir = nullptr;
		{
			auto &files = app_->view_files();
			MutexGuard guard(&files.mutex);
			
			if (IsOnFileNameStringNTS(evt->pos(), &to_dir) != -1 && to_dir->is_dir_or_so()) {
				to_dir = to_dir->Clone();
			} else {
				/// Otherwise drop onto current directory:
				to_dir = io::FileFromPath(files.data.processed_dir_path);
			}
		}
		
		if (to_dir == nullptr) {
			delete files_vec;
			return;
		}
		
//		mtl_info("JJJJJJJJJJJJJJJJJJJJ %ld vs %ld",
//			i64(evt->proposedAction()),
//			i64(evt->possibleActions()));
		FinishDropOperation(files_vec, to_dir, evt->proposedAction(),
			evt->possibleActions());
	}
	
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

void
Table::FinishDropOperation(QVector<io::File*> *files_vec,
	io::File *to_dir, Qt::DropAction drop_action, Qt::DropActions possible_actions)
{
	CHECK_PTR_VOID(files_vec);
	io::socket::MsgBits io_operation = io::socket::MsgFlagsFor(drop_action)
		| io::socket::MsgFlagsForMany(possible_actions);
	//mtl_info("flags: %u", u32(io_operation));
	auto *ba = new ByteArray();
	ba->set_msg_id(io_operation);
	ba->add_string(to_dir->build_full_path());
	ba->add_i32(files_vec->size());
	
	for (io::File *next: *files_vec) {
		ba->add_string(next->build_full_path());
	}
	
	io::socket::SendAsync(ba);
}

io::File*
Table::GetFileAt(const int row)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	auto &vec = files.data.vec;
	if (row < 0 | row >= vec.size())
		return nullptr;
	
	return vec[row]->Clone();
}

io::File*
Table::GetFileAtNTS(const QPoint &pos, const bool clone, int *ret_file_index)
{
	int row = rowAt(pos.y());
	if (row == -1) {
		return nullptr;
	}
	
	io::Files &files = app_->view_files();
	auto &vec = files.data.vec;
	if (row >= vec.size()) {
		return nullptr;
	}
	
	if (ret_file_index != nullptr) {
		*ret_file_index = row;
	}
	
	io::File *file = vec[row];

	return clone ? file->Clone() : file;
}

int
Table::GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file,
	QString *full_path)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);

	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(local_pos, &file);
	
	if (row != -1 && ret_cloned_file != nullptr)
		*ret_cloned_file = file->Clone();
	
	if (full_path != nullptr)
		*full_path = file->build_full_path();

	return row;
}

int
Table::GetFirstSelectedFile(io::File **ret_cloned_file) {
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	int i = 0;
	for (auto *file: files.data.vec) {
		if (file->selected()) {
			if (ret_cloned_file != nullptr)
				*ret_cloned_file = file->Clone();
			return i;
		}
		i++;
	}
	
	return -1;
}

QString
Table::GetFirstSelectedFileFullPath(QString *ext) {
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	for (io::File *file: files.data.vec) {
		if (file->selected()) {
			if (ext != nullptr)
				*ext = file->cache().ext.toString().toLower();
			return file->build_full_path();
		}
	}
	
	return QString();
}

int
Table::GetIconSize() {
	return verticalHeader()->defaultSectionSize() - 12;
}

void
Table::GetSelectedArchives(QVector<QString> &urls)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	for (io::File *next: files.data.vec) {
		if (!next->selected() || !next->is_regular())
			continue;
		
		QString ext = next->cache().ext.toString();
		if (ext.isEmpty())
			continue;
		
		if (ArchiveExtensions.contains(ext)) {
			urls.append(QUrl::fromLocalFile(next->build_full_path()).toString());
		}
	}
}

void
Table::GetSelectedFileNames(QVector<QString> &names, const StringCase str_case)
{
	io::Files &files = app_->view_files();
	MutexGuard guard = files.guard();
	for (io::File *next: files.data.vec) {
		if (next->selected()) {
			switch (str_case) {
			case StringCase::AsIs: {
				names.append(next->name());
				break;
			}
			case StringCase::Lower: {
				names.append(next->name_lower());
				break;
			}
			default: {
				mtl_trace();
			}
			} /// switch()
		}
	}
	
}

int
Table::GetSelectedFilesCount(QVector<QString> *extensions) {
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	int count = 0;
	
	for (io::File *next: files.data.vec) {
		if (next->selected()) {
			if (extensions != nullptr && next->is_regular()) {
				extensions->append(next->cache().ext.toString());
			}
			count++;
		}
	}
	
	return count;
}

void
Table::HandleKeySelect(const bool up)
{
	int row = shift_select_.base_row;
	if (row == -1)
		row = GetFirstSelectedFile(nullptr);
	
	if (row == 0 && up)
		return;
	
	if (!up && row == model_->rowCount() - 1)
		return;
	
	int scroll_to_row = -1;
	
	QVector<int> indices;
	{
		io::Files &files = app_->view_files();
		MutexGuard guard(&files.mutex);
		SelectAllFilesNTS(false, indices);
		const int count = files.data.vec.size();
		if (row == -1) {
			int select_index = up ? 0 : count - 1;
			if (select_index >= 0) {
				indices.append(select_index);
				auto *file = files.data.vec[select_index];
				file->selected(true);
				scroll_to_row = select_index;
			}
		} else {
			int new_select = row + (up ? -1 : 1);
			
			if (new_select >= 0 && new_select < count) {
				auto *file = files.data.vec[row];
				file->selected(false);
				files.data.vec[new_select]->selected(true);
				indices.append(new_select);
				indices.append(row);
				scroll_to_row = new_select;
			}
		}
	}
	
	model_->UpdateIndices(indices);
	
	if (scroll_to_row != -1) {
		ScrollToRow(scroll_to_row);
	}
}

void
Table::HandleKeyShiftSelect(const bool up)
{
	int row = GetFirstSelectedFile(nullptr);
	if (row == -1)
		return;
	
	QVector<int> indices;
	
	if (shift_select_.head_row == -1)
		shift_select_.head_row = shift_select_.base_row;
	
	if (shift_select_.base_row == -1) {
		shift_select_.base_row = row;
		shift_select_.head_row = row;
		io::Files &files = app_->view_files();
		MutexGuard guard(&files.mutex);
		files.data.vec[row]->selected(true);
		indices.append(row);
	} else {
		if (up) {
			if (shift_select_.head_row == 0)
				return;
			shift_select_.head_row--;
		} else {
			const int count = model_->rowCount();
			if (shift_select_.head_row == count -1)
				return;
			shift_select_.head_row++;
		}
		{
			io::Files &files = app_->view_files();
			MutexGuard guard(&files.mutex);
			SelectAllFilesNTS(false, indices);
			SelectFileRangeNTS(shift_select_.base_row, shift_select_.head_row, indices);
		}
	}
	
	model_->UpdateIndices(indices);
	
	if (shift_select_.head_row != -1) {
		QModelIndex index = model()->index(shift_select_.head_row, 0, QModelIndex());
		scrollTo(index);
		///ScrollToRow(shift_select_.head_row);
	}
}

void
Table::HandleMouseRightClickSelection(const QPoint &pos, QVector<int> &indices)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(pos, &file);
	
	if (file == nullptr) {
		SelectAllFilesNTS(false, indices);
	} else {
		if (!file->selected()) {
			SelectAllFilesNTS(false, indices);
			file->selected(true);
			shift_select_.base_row = row;
			indices.append(row);
		}
	}
}

void
Table::HandleMouseSelectionCtrl(const QPoint &pos, QVector<int> &indices) {
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(pos, &file);
	
	if (file != nullptr) {
		file->selected(!file->selected());
		indices.append(row);
	}
}

void
Table::HandleMouseSelectionShift(const QPoint &pos, QVector<int> &indices)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(pos, &file);
	bool on_file = row != -1;
	
	if (on_file) {
		if (shift_select_.base_row == -1) {
			shift_select_.base_row = row;
			file->selected(true);
			indices.append(row);
		} else {
			SelectAllFilesNTS(false, indices);
			SelectFileRangeNTS(shift_select_.base_row, row, indices);
		}
	}
}

void
Table::HandleMouseSelectionNoModif(const QPoint &pos, QVector<int> &indices,
	bool mouse_pressed)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(pos, &file);
	bool on_file = row != -1;
	shift_select_.base_row = row;
	
	if (on_file) {
		if (mouse_pressed) {
			SelectAllFilesNTS(false, indices);
			if (file != nullptr && !file->selected()) {
				file->selected(true);
				indices.append(row);
			}
		} else { // mouse release
//			SelectAllFilesNTS(false, indices);
//			if (file != nullptr) {
//				file->selected(true);
//				indices.append(row);
//			}
		}
	} else {
		SelectAllFilesNTS(false, indices);
	}
}

int
Table::IsOnFileNameStringNTS(const QPoint &local_pos, io::File **ret_file)
{
	i32 col = columnAt(local_pos.x());
	if (col != (int)Column::FileName) {
		return -1;
	}
	io::File *file = GetFileAtNTS(local_pos, false);
	if (file == nullptr)
		return -1;
	
	QFontMetrics fm = fontMetrics();
	int name_w = fm.boundingRect(file->name()).width();
	if (name_w < delegate_->min_name_w())
		name_w = delegate_->min_name_w();
	const int absolute_name_end = name_w + columnViewportPosition(col);
	
	if (local_pos.x() > absolute_name_end + gui::FileNameRelax)
		return -1;
	
	if (ret_file != nullptr)
		*ret_file = file;
	
	return rowAt(local_pos.y());
}

void
Table::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	auto *app = model_->app();
	const auto modifiers = event->modifiers();
	const bool any_modifiers = (modifiers != Qt::NoModifier);
	const bool shift = (modifiers & Qt::ShiftModifier);
	QVector<int> indices;
	
	if (modifiers & Qt::ControlModifier) {
		if (key == Qt::Key_C)
			ActionCopy(indices);
		else if (key == Qt::Key_X)
			ActionCut(indices);
		else if (key == Qt::Key_V) {
			ActionPaste();
		}
	}
	
	if (key == Qt::Key_Return) {
		if (!any_modifiers) {
			io::File *cloned_file = nullptr;
			int row = GetFirstSelectedFile(&cloned_file);
			if (row != -1) {
				app->FileDoubleClicked(cloned_file, Column::FileName);
				indices.append(row);
			}
		}
	} else if (key == Qt::Key_Down) {
		if (shift)
			HandleKeyShiftSelect(false);
		else
			HandleKeySelect(false);
	} else if (key == Qt::Key_Up) {
		if (shift)
			HandleKeyShiftSelect(true);
		else
			HandleKeySelect(true);
	} else if (key == Qt::Key_D) {
		if (any_modifiers)
			return;
		io::File *cloned_file = nullptr;
		int row = GetFirstSelectedFile(&cloned_file);
		if (row != -1)
			app_->DisplayFileContents(row, cloned_file);
		else {
			app_->HideTextEditor();
		}
	} else if (key == Qt::Key_Escape) {
		app_->HideTextEditor();
	}
	
	model_->UpdateIndices(indices);
}

void
Table::keyReleaseEvent(QKeyEvent *evt) {
	bool shift = evt->modifiers() & Qt::ShiftModifier;
	
	if (!shift) {
		shift_select_ = {};
	}
}

void
Table::LaunchFromOpenWithMenu()
{
	QAction *act = qobject_cast<QAction *>(sender());
	QVariant v = act->data();
	DesktopFile *p = (DesktopFile*) v.value<void *>();
	p->Launch(open_with_.full_path, app_->current_dir());
}

void
Table::leaveEvent(QEvent *evt) {
	int row = mouse_over_file_name_;
	mouse_over_file_name_ = -1;
	model_->UpdateSingleRow(row);
}

QPair<int, int>
Table::ListSelectedFiles(QList<QUrl> &list)
{
	auto &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	int num_files = 0;
	int num_dirs = 0;
	
	for (io::File *next: files.data.vec) {
		if (next->selected()) {
			if (next->is_dir())
				num_dirs++;
			else
				num_files++;
			const QString s = next->build_full_path();
			list.append(QUrl::fromLocalFile(s));
		}
	}
	
	return QPair(num_dirs, num_files);
}

void
Table::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTableView::mouseDoubleClickEvent(evt);
	i32 col = columnAt(evt->pos().x());
	auto *app = model_->app();
	
	if (evt->button() == Qt::LeftButton) {
		if (col == i32(Column::Icon)) {
			io::Files &files = app_->view_files();
			io::File *cloned_file = nullptr;
			{
				MutexGuard guard(&files.mutex);
				cloned_file = GetFileAtNTS(evt->pos(), true);
			}
			if (cloned_file != nullptr)
				app->FileDoubleClicked(cloned_file, Column::Icon);
		} else if (col == i32(Column::FileName)) {
			io::File *cloned_file = nullptr;
			int row = GetFirstSelectedFile(&cloned_file);
			if (row != -1)
				app->FileDoubleClicked(cloned_file, Column::FileName);
		}
	}
}

void
Table::mouseMoveEvent(QMouseEvent *evt)
{
	int row = -1;
	{
		io::Files &files = app_->view_files();
		MutexGuard guard(&files.mutex);
		row = IsOnFileNameStringNTS(evt->pos(), nullptr);
	}
	
	bool repaint = false;
	i32 old_row = mouse_over_file_name_;
	if (row != mouse_over_file_name_) {
		repaint = true;
		mouse_over_file_name_ = row;
	}
	
	if (mouse_down_ && (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0)) {
		auto diff = (evt->pos() - drag_start_pos_).manhattanLength();
		if (diff >= QApplication::startDragDistance())
		{
			drag_start_pos_ = {-1, -1};
			StartDragOperation();
		}
	}
	
	if (repaint) {
		QVector<int> rows = {old_row, mouse_over_file_name_};
		model_->UpdateIndices(rows);
	}
}

void
Table::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	mouse_down_ = true;
	
	auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	const bool shift = modif & Qt::ShiftModifier;
	const bool right_click = evt->button() == Qt::RightButton;
	const bool left_click = evt->button() == Qt::LeftButton;
	QVector<int> indices;
	
	if (left_click) {
		if (ctrl)
			HandleMouseSelectionCtrl(evt->pos(), indices);
		else if (shift)
			HandleMouseSelectionShift(evt->pos(), indices);
		else
			HandleMouseSelectionNoModif(evt->pos(), indices, true);
	}
	
	if (left_click) {
		drag_start_pos_ = evt->pos();
	} else {
		drag_start_pos_ = {-1, -1};
	}
	
	if (right_click) {
		{
			HandleMouseRightClickSelection(evt->pos(), indices);
		}
		ShowRightClickMenu(evt->globalPos(), evt->pos());
	}
	
	model_->UpdateIndices(indices);
}

void
Table::mouseReleaseEvent(QMouseEvent *evt)
{
	QTableView::mouseReleaseEvent(evt);
	drag_start_pos_ = {-1, -1};
	mouse_down_ = false;
	
	QVector<int> indices;
	const bool ctrl = evt->modifiers() & Qt::ControlModifier;
	const bool shift = evt->modifiers() & Qt::ShiftModifier;
	
	if (!ctrl && !shift)
		HandleMouseSelectionNoModif(evt->pos(), indices, false);
	
	model_->UpdateIndices(indices);
}

void
Table::paintEvent(QPaintEvent *event)
{
	QTableView::paintEvent(event);
	
	/**
	if (drop_coord_.y() == -1)
		return;
	
	const i32 row_h = rowHeight(0);
	
	if (row_h < 1)
		return;
	
	QPainter painter(viewport());
	QPen pen(QColor(0, 0, 255));
	pen.setWidthF(2.0);
	painter.setPen(pen);
	
	int y = drop_coord_.y();
	
	int rem = y % row_h;
	
	if (rem < row_h / 2)
		y -= rem;
	else
		y += row_h - rem;
	
	if (y > 0)
		y -= 1;
	
	painter.drawLine(0, y, width(), y); */
}

void
Table::ProcessAction(const QString &action)
{
	App *app = model_->app();
	
	if (action == actions::DeleteFiles) {
		int count = GetSelectedFilesCount();
		if (count == 0)
			return;
		
		QString question = "Delete " + QString::number(count) + " files?";
		QMessageBox::StandardButton reply = QMessageBox::question(this,
			"Delete Files", question, QMessageBox::Yes|QMessageBox::No);
		
		if (reply == QMessageBox::Yes)
			model_->DeleteSelectedFiles();
	} else if (action == actions::RenameFile) {
		app->RenameSelectedFile();
	} else if (action == actions::OpenTerminal) {
		app->OpenTerminal();
	} else if (action == actions::RunExecutable) {
		QString ext;
		QString full_path = GetFirstSelectedFileFullPath(&ext);
		if (!full_path.isEmpty()) {
			ExecInfo info = app->QueryExecInfo(full_path, ext);
			if (info.is_elf() || info.is_shell_script())
				app->RunExecutable(full_path, info);
		}
	} else if (action == actions::SwitchExecBit) {
		app->SwitchExecBitOfSelectedFiles();
	}
}

bool
Table::ScrollToAndSelect(QString full_path)
{
	if (full_path.isEmpty())
		return false;
	
	QStringRef path_ref;
	/// first remove trailing '/' or search will fail:
	if (full_path.endsWith('/'))
		path_ref = full_path.midRef(0, full_path.size() - 1);
	else
		path_ref = full_path.midRef(0, full_path.size());
	int row_count = -1;
	int row = -1;
	{
		io::Files &files = app_->view_files();
		MutexGuard guard(&files.mutex);
		auto &vec = files.data.vec;
		row_count = vec.size();
		
		for (int i = 0; i < row_count; i++) {
			QString file_path = vec[i]->build_full_path();
			
			if (file_path == path_ref) {
				row = i;
				break;
			}
		}
	}
	
	CHECK_TRUE((row != -1));
	ScrollToRow(row);
	SelectRowSimple(row);
	shift_select_.base_row = row;
	
	return true;
}

void
Table::ScrollToRow(int row)
{
	if (row < 0)
		return;
	
	const int rh = verticalHeader()->defaultSectionSize();
	const int header_h = horizontalHeader()->height();
	int visible_rows = (height() - header_h) / rh;
	const int diff = height() % rh;
	if (diff > 0)
		visible_rows++;
	
	const int row_count = model_->rowCount();
	int max = (rh * row_count) - height() + header_h;
	if (max < 0) {
		max = 0;
		return; // no need to scroll
	}
	
	auto *vs = verticalScrollBar();
	vs->setMaximum(max);
	int row_at_pixel = rh * row;
	if (row_at_pixel < 0)
		row_at_pixel = 0;
	
	int half_h = (height() - header_h) / 2;
	
	if (row_at_pixel - half_h > 0) {
		row_at_pixel -= half_h;
		vs->setValue(row_at_pixel);
	} else {
		vs->setValue(0);
	}
}

void
Table::ScrollToAndSelectRow(const int row, const bool deselect_others) {
	ScrollToRow(row);
	SelectRowSimple(row, deselect_others);
}

void
Table::SelectByLowerCase(QVector<QString> filenames)
{
	if (filenames.isEmpty())
		return;
	
	QVector<int> indices;
	int ni, fi = 0;
	QString full_path;
	io::Files &files = app_->view_files();
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		for (io::File *next: vec) {
			int count = filenames.size();
			if (count == 0)
				break;
			ni = 0;
			for (const QString &name: filenames) {
				if (next->name_lower() == name) {
					if (full_path.isEmpty()) {
						full_path = next->build_full_path();
					}
					next->selected(true);
					indices.append(fi);
					filenames.remove(ni);
					break;
				}
				ni++;
			}
		}
		fi++;
	}
	
	model_->UpdateIndices(indices);
	ScrollToAndSelect(full_path);
}

void
Table::SelectAllFilesNTS(const bool flag, QVector<int> &indices) {
	int i = 0;
	io::Files &files = app_->view_files();
	for (auto *file: files.data.vec) {
		if (file->selected() != flag) {
			indices.append(i);
			file->selected(flag);
		}
		i++;
	}
}

void
Table::SelectFileRangeNTS(const int row1, const int row2, QVector<int> &indices)
{
	io::Files &files = app_->view_files();
	QVector<io::File*> &vec = files.data.vec;
	
	if (row1 < 0 || row1 >= vec.size() || row2 < 0 || row2 >= vec.size()) {
///		mtl_warn("row1: %d, row2: %d", row1, row2);
		return;
	}
	
	int row_start = row1;
	int row_end = row2;
	if (row_start > row_end) {
		row_start = row2;
		row_end = row1;
	}
	
	for (int i = row_start; i <= row_end; i++) {
		vec[i]->selected(true);
		indices.append(i);
	}
}

int
Table::SelectNextRow(const int relative_offset,
	const bool deselect_all_others, QVector<int> &indices)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	auto &vec = files.data.vec;
	int i = 0, ret_val = -1;
	
	for (io::File *next: vec) {
		if (next->selected()) {
			int n = i + relative_offset;
			if (n >= 0 && n < vec.size()) {
				vec[n]->selected(true);
				ret_val = n;
				break;
			}
		}
		i++;
	}
	
	if (deselect_all_others) {
		i = -1;
		for (io::File *next: vec) {
			i++;
			if (i == ret_val)
				continue;
			if (next->selected()) {
				next->selected(false);
				indices.append(i);
			}
		}
	}
	
	if (ret_val != -1) {
		indices.append(ret_val);
		return ret_val;
	}
	
	if (vec.isEmpty())
		return -1;
	
	if (relative_offset >= 0) {
		i = vec.size() - 1;
		vec[i]->selected(true);
		indices.append(i);
		return i;
	}
	
	vec[0]->selected(true);
	indices.append(0);
	
	return 0;
}

void
Table::SelectRowSimple(const int row, const bool deselect_others)
{
	io::Files &files = app_->view_files();
	QVector<int> indices;
	{
		MutexGuard guard(&files.mutex);
		auto &vec = files.data.vec;
		
		if (deselect_others) {
			int i = 0;
			for (io::File *next: vec) {
				if (next->selected()) {
					next->selected(false);
					indices.append(i);
				}
				i++;
			}
		}
		
		if (row >= 0 && row < vec.size()) {
			vec[row]->selected(true);
			indices.append(row);
		}
	}
	
	model_->UpdateIndices(indices);
}

void
Table::SetCustomResizePolicy() {
	auto *hh = horizontalHeader();
	hh->setSectionResizeMode(i8(gui::Column::Icon), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::FileName), QHeaderView::Stretch);
	hh->setSectionResizeMode(i8(gui::Column::Size), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::TimeCreated), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::TimeModified), QHeaderView::Fixed);
	
	QFontMetrics fm = fontMetrics();
	QString sample_date = QLatin1String("2020-12-01 18:04");
	
	const int icon_col_w = fm.boundingRect(QLatin1String("Steam")).width();
	const int size_col_w = fm.boundingRect(QLatin1String("1023.9 GiB")).width() + 2;
	const int time_col_w = fm.boundingRect(sample_date).width() + 10;
	
	setColumnWidth(i8(gui::Column::Icon), icon_col_w);
	setColumnWidth(i8(gui::Column::Size), size_col_w);
	setColumnWidth(i8(gui::Column::TimeCreated), time_col_w);
	setColumnWidth(i8(gui::Column::TimeModified), time_col_w);
}

void
Table::ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos)
{
	indices_.clear();
	QVector<QString> extensions;
	const int selected_count = GetSelectedFilesCount(&extensions);
	QMenu *menu = new QMenu();
	menu->setAttribute(Qt::WA_DeleteOnClose);
	
	QString dir_full_path;
	QString file_under_mouse_full_path;
	{
		io::File *file = nullptr;
		if (GetFileUnderMouse(local_pos, &file) != -1) {
			cornus::AutoDelete ad(file);
			file_under_mouse_full_path = file->build_full_path();
			if (file->is_dir()) {
				dir_full_path = file_under_mouse_full_path;
			}
		}
	}
	
	if (selected_count == 1) {
		if (!file_under_mouse_full_path.isEmpty())
			AddOpenWithMenuTo(menu, file_under_mouse_full_path);
	}
	
	QMenu *new_menu = app_->CreateNewMenu();
	menu->addMenu(new_menu);
	menu->addSeparator();
	
	if (selected_count > 0)
	{ // cut copy
		QAction *action = menu->addAction(tr("Cut"));
		connect(action, &QAction::triggered, [this] {
			ActionCut(this->indices_);
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-cut")));
		
		action = menu->addAction(tr("Copy"));
		connect(action, &QAction::triggered, [this] {
			ActionCopy(this->indices_);
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
	}
	
	const Clipboard &clipboard = app_->clipboard();
	if (clipboard.has_files())
	{
		QString file_count_str = QLatin1String(" (")
			+ QString::number(clipboard.file_count()) + ')';
		{ // paste
			QAction *action = menu->addAction(tr("Paste") + file_count_str);
			connect(action, &QAction::triggered, [this] {
				ActionPaste();
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-paste")));
		}
		{
			QAction *action = menu->addAction(tr("Paste Link") + file_count_str);
			connect(action, &QAction::triggered, [this] {
				ActionPasteLinks();
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-paste")));
		}
		menu->addSeparator();
	}
	
	if (selected_count > 0) {
		QAction *action = menu->addAction(tr("Delete Files"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteFiles);});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
		menu->addSeparator();
	}
	
	if (selected_count > 0) {
		QAction *action = menu->addAction(tr("Rename File"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::RenameFile);});
		action->setIcon(QIcon::fromTheme(QLatin1String("insert-text")));
	}
	
	if (selected_count > 0) {
		QAction *action = menu->addAction(tr("Run Executable"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::RunExecutable);});
		action->setIcon(QIcon::fromTheme(QLatin1String("system-run")));
	}
	
	{
		QAction *action = menu->addAction(tr("Open Terminal"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::OpenTerminal);});
		action->setIcon(QIcon::fromTheme(QLatin1String("utilities-terminal")));
	}
	
	if (selected_count > 0) {
		QAction *action = menu->addAction(tr("Switch Exec Bit"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::SwitchExecBit);});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-undo")));
	}
	
	if (AnyArchive(extensions)) {
		QMenu *extract = new QMenu(tr("Extract"));
		menu->addSeparator();
		menu->addMenu(extract);
		QIcon *icon = app_->GetIcon(QLatin1String("zip"));
		if (icon != nullptr)
			extract->setIcon(*icon);
		menu->addSeparator();
		
		{
			QAction *action = extract->addAction(tr("Extract Here"));
			connect(action, &QAction::triggered, [=] {
				app_->ExtractTo(app_->current_dir());
			});
		}
		
		{
			QAction *action = extract->addAction(tr("Extract To.."));
			connect(action, &QAction::triggered, [=] { app_->ExtractAskDestFolder(); });
		}
	}
	
	QString count_folder = dir_full_path.isEmpty() ? app_->current_dir() : dir_full_path;
	
	if (count_folder != QLatin1String("/")) {
		QAction *action = menu->addAction(tr("Count Folder Size"));
		
		connect(action, &QAction::triggered, [=] {
			gui::CountFolder cf(app_, count_folder);
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("emblem-important")));
	}
	
	model_->UpdateIndices(indices_);
	menu->popup(global_pos);
}

void
Table::ShowVisibleColumnOptions(QPoint pos)
{
	QMenu *menu = new QMenu();
	auto *hz = horizontalHeader();
	
	for (int i = (int)Column::FileName + 1; i < int(Column::Count); i++) {
		QVariant v = model_->headerData(i, Qt::Horizontal, Qt::DisplayRole);
		QString name = v.toString();
		
		QAction *action = menu->addAction(name);
		action->setCheckable(true);
		action->setChecked(!hz->isSectionHidden(i));
		
		connect(action, &QAction::triggered, [=] {
			hz->setSectionHidden(i, !action->isChecked());
			app_->prefs().Save();
		});
	}
	
	menu->popup(QCursor::pos());
}

void
Table::SortingChanged(int logical, Qt::SortOrder order) {
	io::SortingOrder sorder = {Column(logical), order == Qt::AscendingOrder};
	io::Files &files = app_->view_files();
	int file_count;
	{
		MutexGuard guard(&files.mutex);
		file_count = files.data.vec.size();
		files.data.sorting_order = sorder;
		std::sort(files.data.vec.begin(), files.data.vec.end(), cornus::io::SortFiles);
	}
	model_->UpdateVisibleArea();
}

void
Table::StartDragOperation()
{
	QMimeData *mimedata = new QMimeData();
	QList<QUrl> urls;
	QPair<int, int> files_folders = ListSelectedFiles(urls);
	if (urls.isEmpty())
		return;
	
	mimedata->setUrls(urls);
	
/// Set a pixmap that will be shown alongside the cursor during the operation:
	
	const int img_w = 128;
	const int img_h = img_w / 2;
	QPixmap pixmap(QSize(img_w, img_h));
	QPainter painter(&pixmap);
	
	QRect r(0, 0, img_w, img_h);
	painter.fillRect(r, QColor(235, 235, 255));
	
	QPen pen(QColor(0, 0, 0));
	painter.setPen(pen);
	
	QString dir_str = QString::number(files_folders.first)
		+ QString(" folder(s)");
	QString file_str = QString::number(files_folders.second)
		+ QString(" file(s)");
	auto str_rect = fontMetrics().boundingRect(dir_str);
	
	auto r2 = r;
	r2.setY(r2.y() + str_rect.height());
	
	painter.drawText(r, Qt::AlignCenter + Qt::AlignVCenter, dir_str, &r);
	painter.drawText(r2, Qt::AlignCenter + Qt::AlignVCenter, file_str, &r2);

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimedata);
	drag->setPixmap(pixmap);
	drag->exec(Qt::CopyAction | Qt::MoveAction);
}

void
Table::SyncWith(const cornus::Clipboard &cl, QVector<int> &indices)
{
	auto &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	
	QVector<QString> file_paths = cl.file_paths;
	QString dir_path = files.data.processed_dir_path;
	
	for (int i = file_paths.size() - 1; i >= 0; i--)
	{
		const QString full_path = QDir::cleanPath(file_paths[i]);
		auto name = io::GetFileNameOfFullPath(full_path).toString();
		
		if (name.isEmpty() || (dir_path + name != full_path)) {
			file_paths.remove(i);
			continue;
		}
		
		file_paths[i] = name;
	}
	
	if (file_paths.isEmpty())
		return;
	
	io::FileBits flag = io::FileBits::Empty;
	if (cl.action == ClipboardAction::Cut) {
///		mtl_info("Cut");
		flag = io::FileBits::ActionCut;
	} else if (cl.action == ClipboardAction::Copy) {
///		mtl_info("Copy");
		flag = io::FileBits::ActionCopy;
	} else if (cl.action == ClipboardAction::Paste) {
		mtl_info("Paste");
		flag = io::FileBits::ActionPaste;
	} else if (cl.action == ClipboardAction::Link) {
		mtl_info("Link");
		flag = io::FileBits::PasteLink;
	}
	
	int i = -1;
	for (io::File *next: files.data.vec)
	{
		i++;
		bool added = false;
		if (next->clear_all_actions_if_needed()) {
			indices.append(i);
			added = true;
		}
		
		const int count = file_paths.size();
		for (int k = 0; k < count; k++) {
			if (file_paths[k] == next->name()) {
				if (!added) {
					indices.append(i);
				}
				next->toggle_flag(flag, true);
				file_paths.remove(k);
				break;
			}
		}
	}
}

void
Table::UpdateLineHeight() {
	auto fm = fontMetrics();
	int str_h = fm.height();
	int ln = str_h * 1.3;
	auto *vh = verticalHeader();
	vh->setMinimumSectionSize(str_h);
	vh->setMaximumSectionSize(ln);
	vh->setSectionResizeMode(QHeaderView::Fixed);
	vh->setDefaultSectionSize(ln);
	vh->setSectionsMovable(false);
}

} // cornus::gui::


