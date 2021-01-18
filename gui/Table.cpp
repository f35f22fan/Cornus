#include "Table.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "TableDelegate.hpp"
#include "TableModel.hpp"

#include <map>

#include <QAbstractItemView>
#include <QAction>
#include <QClipboard>
#include <QDialog>
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

Table::Table(TableModel *tm, App *app) : app_(app),
table_model_(tm)
{
	setModel(table_model_);
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
		table_model_->app()->DisplayFileContents(index);
	});
	setDragEnabled(true);
	setAcceptDrops(true);
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	setIconSize(QSize(32, 32));
	resizeColumnsToContents();
	//setShowGrid(false);
	//setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::NoSelection);//QAbstractItemView::ExtendedSelection);
//	setDragDropOverwriteMode(false);
//	setDropIndicatorShown(true);
	
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

Table::~Table() {
	delete table_model_;
}

void
Table::dragEnterEvent(QDragEnterEvent *event)
{
	const QMimeData *mimedata = event->mimeData();
	
	if (mimedata->hasUrls())
		event->acceptProposedAction();
}

void
Table::dragLeaveEvent(QDragLeaveEvent *event)
{
	drop_y_coord_ = -1;
	update();
}

void
Table::dragMoveEvent(QDragMoveEvent *event)
{
	const auto &pos = event->pos();
	drop_y_coord_ = pos.y();
	
	// repaint() or update() don't work because
	// the window is not raised when dragging a song
	// on top of the playlist and the repaint
	// requests are ignored.
	// repaint(0, y - h / 2, width(), y + h / 2);
	// using a hack:
	int row = rowAt(pos.y());
	
	if (row != -1)
		table_model_->UpdateSingleRow(row);
}

void
Table::dropEvent(QDropEvent *event)
{
	drop_y_coord_ = -1;
	App *app = table_model_->app();
	
	if (event->mimeData()->hasUrls()) {
		/**
		gui::Playlist *playlist = app->GetComboCurrentPlaylist();
		CHECK_PTR_VOID(playlist);
		QVector<io::File> files;
		
		for (const QUrl &url: event->mimeData()->urls())
		{
			QString path = url.path();
			io::File file;
			
			if (io::FileFromPath(file, path) == io::Err::Ok)
				files.append(file);
		}
		
		int index = 0;
		const int row_h = rowHeight(0);
		int drop_at_y = event->pos().y() + verticalScrollBar()->sliderPosition();;
		
		if (row_h > 0 && drop_at_y > 0)
		{
			int rem = drop_at_y % row_h;
			
			if (rem < row_h / 2)
				drop_at_y -= rem;
			else
				drop_at_y += row_h - rem;
			
			index = drop_at_y / row_h;
		}
		
		if (index != -1) {
			mtl_trace();
		//	app->AddFilesToPlaylist(files, playlist, index);
		} **/
	}
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
	if (row == -1)
		return nullptr;
	
	io::Files &files = app_->view_files();
	auto &vec = files.data.vec;
	if (row >= vec.size())
		return nullptr;
	
	if (ret_file_index != nullptr)
		*ret_file_index = row;
	
	io::File *file = vec[row];
	return clone ? file->Clone() : file;
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

int
Table::IsOnFileNameStringNTS(const QPoint &pos, io::File **ret_file)
{
	i32 col = columnAt(pos.x());
	if (col != (int)Column::FileName)
		return -1;
	
	io::File *file = GetFileAtNTS(pos, false);
	if (file == nullptr)
		return -1;
	
	QFontMetrics fm = fontMetrics();
	const int name_w = fm.boundingRect(file->name()).width();
	const int absolute_name_end = name_w + columnViewportPosition(col);
	
	if (absolute_name_end < pos.x())
		return -1;
	
	if (ret_file != nullptr)
		*ret_file = file;
	
	return rowAt(pos.y());
}

void
Table::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	auto *app = table_model_->app();
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
	}
	table_model_->UpdateIndices(indices);
}

void
Table::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTableView::mouseDoubleClickEvent(evt);
	i32 col = columnAt(evt->pos().x());
	auto *app = table_model_->app();
	
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
Table::mousePressEvent(QMouseEvent *evt)
{
	auto modif = evt->modifiers();
	const bool ctrl_pressed = modif & Qt::ControlModifier;
	QVector<int> indices;
	
	io::Files &files = app_->view_files();
	if (!ctrl_pressed) {
		MutexGuard guard(&files.mutex);
		SelectAllFilesNTS(false, indices);
	}
	
	{
		MutexGuard guard(&files.mutex);
		io::File *file = nullptr;
		int row = IsOnFileNameStringNTS(evt->pos(), &file);
		if (row >= 0) {
			file->selected(true);
			indices.append(row);
		}
	}
	
	if (evt->button() == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPos());
	}
	if (evt->button() == Qt::LeftButton) {
		QTableView::mousePressEvent(evt);
	}
	
	table_model_->UpdateIndices(indices);
}

void
Table::paintEvent(QPaintEvent *event)
{
//	static int cc = 0;
//	printf("%d \n", cc++);
	QTableView::paintEvent(event);
	
	if (drop_y_coord_ == -1)
		return;
	
	const i32 row_h = rowHeight(0);
	
	if (row_h < 1)
		return;
	
	QPainter painter(viewport());
	QPen pen(QColor(0, 0, 255));
	pen.setWidthF(2.0);
	painter.setPen(pen);
	
	const i32 slider_pos = verticalScrollBar()->sliderPosition();
	int y = drop_y_coord_;// - slider_pos;
	
	int rem = y % row_h;
	
	if (rem < row_h / 2)
		y -= rem;
	else
		y += row_h - rem;
	
	y -= slider_pos;
	
	if (y > 0)
		y -= 1;
	
	//mtl_info("y: %d, slider: %d", y, slider_pos);
	painter.drawLine(0, y, width(), y);
}

void
Table::ProcessAction(const QString &action)
{
	App *app = table_model_->app();
	
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
			table_model_->DeleteSelectedFiles();
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
Table::resizeEvent(QResizeEvent *event) {
	QTableView::resizeEvent(event);
	double w = event->size().width();
	const int icon = 50;
	const int size = 110;
	QFontMetrics metrics = fontMetrics();
	QString sample_date = QLatin1String("2020-12-01 18:04");
	const int time_w = metrics.boundingRect(sample_date).width() * 1.1;
	int file_name = w - (icon + size + time_w + 5);
	
	setColumnWidth(i8(gui::Column::Icon), icon);// 45);
	setColumnWidth(i8(gui::Column::FileName), file_name);// 500);
	setColumnWidth(i8(gui::Column::Size), size);
	setColumnWidth(i8(gui::Column::TimeCreated), time_w);
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
Table::ScrollToRow(const int row) {
	const int rh = verticalHeader()->defaultSectionSize();
	int visible_rows = height() / rh;
	int total = rh * (row - visible_rows / 2); /// scroll to middle
	if (total < 0)
		total = 0;
	
	const int max = (table_model_->rowCount() - visible_rows + 1) * rh;
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
		}
		file->selected(flag);
		i++;
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
	
	table_model_->UpdateIndices(indices);
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
Table::ShowRightClickMenu(const QPoint &pos)
{
	const int selected_count = GetSelectedFilesCount();
	App *app = table_model_->app();
	QMenu *menu = new QMenu();
	
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
	
	menu->popup(pos);
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
	table_model_->UpdateRowRange(0, file_count - 1);
}

} // cornus::gui::

