#include "Table.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "../io/socket.hh"
#include "CountFolder.hpp"
#include "TableDelegate.hpp"
#include "TableModel.hpp"

#include <map>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
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

const int FileNameRelax = 2;

Table::Table(TableModel *tm, App *app) : app_(app),
model_(tm)
{
	setModel(model_);
	delegate_ = new TableDelegate(this, app_);
	setAlternatingRowColors(false);
	auto d = static_cast<QAbstractItemDelegate*>(delegate_);
	setItemDelegate(d);
	auto *hz = horizontalHeader();
	hz->setSortIndicatorShown(true);
	hz->setSectionHidden(int(Column::TimeModified), true);
	hz->setSortIndicator(int(Column::FileName), Qt::AscendingOrder);
	connect(hz, &QHeaderView::sortIndicatorChanged, this, &Table::SortingChanged);
	//horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	horizontalHeader()->setSectionsMovable(false);
	verticalHeader()->setSectionsMovable(false);
	connect(verticalHeader(), &QHeaderView::sectionClicked, [=](int index) {
		model_->app()->DisplayFileContents(index);
	});
	{
		setDragEnabled(true);
		setAcceptDrops(true);
		setDragDropOverwriteMode(false);
		setDropIndicatorShown(true);
	}
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	resizeColumnsToContents();
	///setShowGrid(false);
	setSelectionMode(QAbstractItemView::NoSelection);//ExtendedSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	
	int sz = GetIconSize();
	setIconSize(QSize(sz, sz));
}

Table::~Table() {
	delete model_;
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

void
Table::dragEnterEvent(QDragEnterEvent *evt)
{
	const QMimeData *mimedata = evt->mimeData();
	
	if (mimedata->hasUrls()) {
		evt->accept();
		evt->acceptProposedAction();
	}
	
	///QByteArray ba = mimedata->data();
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
	if (evt->mimeData()->hasUrls()) {
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
		
		mtl_info("JJJJJJJJJJJJJJJJJJJJ %ld vs %ld",
			i64(evt->proposedAction()),
			i64(evt->possibleActions()));
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
	io::socket::MsgType io_operation = io::socket::MsgFlagsFor(drop_action)
		| io::socket::MsgFlagsForMany(possible_actions);
	mtl_info("flags: %u", u32(io_operation));
	auto *ba = new ByteArray();
	ba->add_msg_type(io_operation);
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
Table::GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file)
{
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);

	io::File *file = nullptr;
	int row = IsOnFileNameStringNTS(local_pos, &file);
	
	if (row != -1)
		*ret_cloned_file = file->Clone();

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
	return verticalHeader()->defaultSectionSize();
}

int
Table::GetSelectedFilesCount() {
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	int count = 0;
	
	for (io::File *next: files.data.vec) {
		if (next->selected())
			count++;
	}
	
	return count;
}

void
Table::HandleMouseSelection(QMouseEvent *evt, QVector<int> &indices)
{
	auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	const bool shift = modif & Qt::ShiftModifier;
	const bool right_click = evt->button() == Qt::RightButton;
	
	io::Files &files = app_->view_files();
	{
		MutexGuard guard(&files.mutex);
		
		if (!ctrl && !right_click) {
			SelectAllFilesNTS(false, indices);
		}
		
		if (shift) {
			int row = rowAt(evt->pos().y());
			SelectFileRangeNTS(shift_select_.starting_row, row, indices);
			model_->UpdateIndices(indices);
			
			return;
		}
		
		io::File *file = nullptr;
		int row = IsOnFileNameStringNTS(evt->pos(), &file);
		if (row >= 0) {
			shift_select_.starting_row = row;
			shift_select_.do_restart = true;
			if (ctrl)
				file->selected(!file->selected());
			else
				file->selected(true);
			indices.append(row);
		} else {
			shift_select_.starting_row = -1;
		}
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
	const int name_w = fm.boundingRect(file->name()).width();
	const int absolute_name_end = name_w + columnViewportPosition(col);
	
	if (local_pos.x() > absolute_name_end + FileNameRelax)
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
	
	if (key == Qt::Key_Return) {
		if (any_modifiers)
			return;
		io::File *cloned_file = nullptr;
		int row = GetFirstSelectedFile(&cloned_file);
		if (row != -1) {
			app->FileDoubleClicked(cloned_file, Column::FileName);
			indices.append(row);
		}
	} else if (key == Qt::Key_Down) {
		const bool deselect_all_others = !shift;
		int row = SelectNextRow(1, deselect_all_others, indices);
		if (row != -1) {
			QModelIndex index = model()->index(row, 0, QModelIndex());
			scrollTo(index);
		}
	} else if (key == Qt::Key_Up) {
		const bool deselect_all_others = !shift;
		int row = SelectNextRow(-1, deselect_all_others, indices);
		if (row != -1) {
			QModelIndex index = model()->index(row, 0, QModelIndex());
			scrollTo(index);
		}
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
	if (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0) {
		auto diff = (evt->pos() - drag_start_pos_).manhattanLength();
		if (diff >= QApplication::startDragDistance())
		{
			StartDragOperation();
		}
	}
}

void
Table::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	
	if (evt->button() == Qt::LeftButton) {
		drag_start_pos_ = evt->pos();
	} else {
		drag_start_pos_ = {-1, -1};
	}
	
	QVector<int> indices;
	HandleMouseSelection(evt, indices);
	
	if (evt->button() == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPos(), evt->pos());
	}
	
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
	
	if (action == actions::CreateNewFile) {
		app->AskCreateNewFile(
			io::File::NewTextFile(app->current_dir(), "File.txt"),
			tr("Create New File"));
	} else if (action == actions::CreateNewFolder) {
		app->AskCreateNewFile(
			io::File::NewFolder(app->current_dir(), "New Folder"),
			tr("Create New Folder"));
	} else if (action == actions::DeleteFiles) {
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
			if (info.is_elf() || info.is_script())
				app->RunExecutable(full_path, info);
		}
	} else if (action == actions::SwitchExecBit) {
		app->SwitchExecBitOfSelectedFiles();
	}
}

void
Table::resizeEvent(QResizeEvent *event)
{
	QTableView::resizeEvent(event);
	QFontMetrics fm = fontMetrics();
	QString sample_date = QLatin1String("2020-12-01 18:04");
	
	const int w = event->size().width();
	const int icon_col_w = fm.boundingRect(QLatin1String("Steam")).width();
	const int size_col_w = fm.boundingRect(QLatin1String("1023.9 GiB")).width() + 10;
	const int time_col_w = fm.boundingRect(sample_date).width() + 10;
	const int filename_col_w = w - (icon_col_w + size_col_w + time_col_w) - 3;
	
	setColumnWidth(i8(gui::Column::Icon), icon_col_w);
	setColumnWidth(i8(gui::Column::FileName), filename_col_w);
	setColumnWidth(i8(gui::Column::Size), size_col_w);
	setColumnWidth(i8(gui::Column::TimeCreated), time_col_w);
}

bool
Table::ScrollToAndSelect(QString full_path)
{
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
	
	if (row == -1) {
		mtl_trace();
		return false;
	}
	
	ScrollToRow(row);
	SelectRowSimple(row);
	
	return true;
}

void
Table::ScrollToRow(int row) {
	const int rh = verticalHeader()->defaultSectionSize();
	int visible_rows = height() / rh;
	const int row_count = model_->rowCount();
//	mtl_info("row: %d, visible_rows: %d, row_count: %d",
//		row, visible_rows, row_count);
	row -= visible_rows / 2;
	
	int total = rh * row;
	if (total < 0)
		total = 0;
	
	int max = (row_count - visible_rows) * rh;
	if (max < 0) {
		max = 0;
		return; // no need to scroll
	}
	auto *vs = verticalScrollBar();
	vs->setMaximum(max);
	vs->setValue(total);
}

void
Table::ScrollToAndSelectRow(const int row, const bool deselect_others) {
	ScrollToRow(row);
	SelectRowSimple(row, deselect_others);
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
	
///	mtl_warn("row_start: %d, row_end: %d", row_start, row_end);
	
	for (int i = row_start; i <= row_end; i++) {
		vec[i]->selected(true);
		indices.append(i);
	}
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
Table::ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos)
{
	const int selected_count = GetSelectedFilesCount();
	App *app = model_->app();
	QMenu *menu = new QMenu();
	QString dir_full_path;
	{
		io::File *file = nullptr;
		if (GetFileUnderMouse(local_pos, &file) != -1) {
			cornus::AutoDelete ad(file);
			if (file->is_dir()) {
				dir_full_path = file->build_full_path();
			}
		}
	}
	
	{
		QAction *action = menu->addAction(tr("New Folder"));
		auto *icon = app->GetIcon(QLatin1String("special_folder"));
		if (icon != nullptr)
			action->setIcon(*icon);
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::CreateNewFolder);});
	}
	
	{
		QAction *action = menu->addAction(tr("New Text File"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::CreateNewFile);});
		auto *icon = app->GetIcon(QLatin1String("text"));
		if (icon != nullptr)
			action->setIcon(*icon);
	}
	
	if (selected_count > 0) {
		QAction *action = menu->addAction(tr("Delete Files"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteFiles);});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
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
	
	QString count_folder = dir_full_path.isEmpty() ? app_->current_dir() : dir_full_path;
	
	if (count_folder != QLatin1String("/")) {
		QAction *action = menu->addAction(tr("Count Folder Size"));
		
		connect(action, &QAction::triggered, [=] {
			gui::CountFolder cf(app_, count_folder);
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("emblem-important")));
	}
	
	menu->popup(global_pos);
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
	model_->UpdateRowRange(0, file_count - 1);
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
	
/** Qt::CopyAction	0x1	Copy the data to the target.
Qt::MoveAction	0x2	Move the data from the source to the target.
Qt::LinkAction	0x4	Create a link from the source to the target.
Qt::ActionMask	0xff	 
Qt::IgnoreAction	0x0	Ignore the action (do nothing with the data).
Qt::TargetMoveAction	0x8002	On Windows, this value is used when
 the ownership of the D&D data should be taken over by the target
 application, i.e., the source application should not delete the data.
 On X11 this value is used to do a move. TargetMoveAction is not
 used on the Mac.  */
}

} // cornus::gui::

