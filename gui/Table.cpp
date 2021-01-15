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

Table::Table(TableModel *tm) :
table_model_(tm)
{
	setModel(table_model_);
	delegate_ = new TableDelegate(this);
	auto d = static_cast<QAbstractItemDelegate*>(delegate_);
	setItemDelegateForColumn(int(Column::Icon), d);
	setItemDelegateForColumn(int(Column::FileName), d);
	setItemDelegateForColumn(int(Column::Size), d);
	setItemDelegateForColumn(int(Column::TimeCreated), d);
	setItemDelegateForColumn(int(Column::TimeModified), d);
	auto *hz = horizontalHeader();
	hz->setSortIndicatorShown(true);
	hz->setSectionHidden(int(Column::TimeModified), true);
	hz->setSortIndicator(int(Column::FileName), Qt::AscendingOrder);
	connect(hz, &QHeaderView::sortIndicatorChanged, this, &Table::SortingChanged);
	//horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	horizontalHeader()->setSectionsMovable(true);
	verticalHeader()->setSectionsMovable(false);
	setDragEnabled(true);
	setAcceptDrops(true);
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	setIconSize(QSize(32, 32));
	resizeColumnsToContents();
	//setShowGrid(false);
//	setDragDropOverwriteMode(false);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
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
	QScrollBar *vscroll = verticalScrollBar();
	drop_y_coord_ = pos.y() + vscroll->sliderPosition();
	
	// repaint() or update() don't work because
	// the window is not raised when dragging a song
	// on top of the playlist and the repaint
	// requests are ignored.
	// repaint(0, y - h / 2, width(), y + h / 2);
	// using a hack:
	int row = rowAt(pos.y());
	
	if (row != -1)
		table_model_->UpdateRangeDefault(row);
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
Table::GetFirstSelectedFile(int *ret_row_index)
{
	const int row_index = GetFirstSelectedRowIndex();
	if (row_index == -1)
		return nullptr;
	
	io::Files *files = table_model_->files();
	MutexGuard guard(&files->mutex);
	auto &vec = files->vec;
	
	if (row_index >= vec.size())
		return nullptr;
	
	if (ret_row_index != nullptr)
		*ret_row_index = row_index;
	
	return vec[row_index]->Clone();
}

int
Table::GetFirstSelectedRowIndex() {
	QItemSelectionModel *select = selectionModel();
	
	if (!select->hasSelection())
		return -1;
	
	QModelIndexList rows = select->selectedRows();
	
	if (rows.isEmpty())
		return -1;
	
	return rows[0].row();
}

void
Table::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	auto *app = table_model_->app();
	const auto modifiers = event->modifiers();
	const bool no_modifiers = (modifiers == Qt::NoModifier);
	
	if (no_modifiers) {
		if (key == Qt::Key_Return) {
			io::File *file = GetFirstSelectedFile();
			if (file == nullptr)
				return;
			app->FileDoubleClicked(file, Column::FileName);
		} else if (key == Qt::Key_Down) {
			SelectNextRow(1);
		} else if (key == Qt::Key_Up) {
			SelectNextRow(-1);
		}
	}
}

void
Table::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTableView::mouseDoubleClickEvent(evt);
	
	i32 col = columnAt(evt->pos().x());
	auto *app = table_model_->app();
	io::File *file = GetFirstSelectedFile();
	
	if (evt->button() == Qt::LeftButton) {
		if (col == i32(Column::Icon)) {
			app->FileDoubleClicked(file, Column::Icon);
		} else if (col == i32(Column::FileName)) {
			app->FileDoubleClicked(file, Column::FileName);
		}
	}
}

void
Table::mousePressEvent(QMouseEvent *evt)
{
	if (evt->button() == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPos());
	}
	if (evt->button() == Qt::LeftButton) {
		QTableView::mousePressEvent(evt);
	}
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
		QItemSelectionModel *select = selectionModel();
		
		if (!select->hasSelection())
			return;
		
		QModelIndexList rows = select->selectedRows();
		if (rows.size() == 0)
			return;
		
		QString question = "Delete " + QString::number(rows.size()) + " files?";
		QMessageBox::StandardButton reply = QMessageBox::question(this,
			"Delete Files", question, QMessageBox::Yes|QMessageBox::No);
		
		if (reply == QMessageBox::Yes)
			table_model_->DeleteSelectedFiles();
	}
}

void
Table::RemoveSongsAndDeleteFiles(const QModelIndexList &indices)
{
	/**
	const Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
	bool confirm_delete = (mods & Qt::ShiftModifier) == 0;
	
	const QString url_prefix = QLatin1String("file://");
	std::map<int, Song*> song_map;
	QVector<Song*> &songs = table_model_->songs();
	
	for (QModelIndex row: indices) {
		const int row_index = row.row();
		song_map[row_index] = songs[row_index];
	}
	
	for (auto iter = song_map.rbegin(); iter != song_map.rend(); ++iter)
	{
		Song *song = iter->second;
		int row = iter->first;
		QUrl url(song->uri());
		
		if (!url.isLocalFile())
			continue;
		
		if (confirm_delete) {
			QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm",
				"Delete file(s)?", QMessageBox::Yes | QMessageBox::No);
			
			if (reply != QMessageBox::Yes) {
				return;
			}
			
			confirm_delete = false;
		}
		
		QString full_path = url.toLocalFile();
		
		if (full_path.startsWith(url_prefix))
			full_path = full_path.mid(url_prefix.size());
		
		auto path_ba = full_path.toLocal8Bit();
		
		if (remove(path_ba.data()) != 0)
			mtl_status(errno);
		
		table_model_->removeRows(row, 1, QModelIndex());
	} **/
}

void
Table::resizeEvent(QResizeEvent *event) {
	QTableView::resizeEvent(event);
	double w = event->size().width();
	const int icon = 50;
	const int size = 110;
	QFont font;
	QFontMetrics metrics(font);
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
	
	int row = -1;
	{
		auto *files = table_model_->files();
		MutexGuard guard(&files->mutex);
		auto &vec = files->vec;
		
		for (int i = 0; i < vec.size(); i++) {
			QString file_path = vec[i]->build_full_path();
			
			if (file_path == path_ref) {
				row = i;
				break;
			}
		}
	}
	
	if (row == -1)
		return false;
	
	QModelIndex index = model()->index(row, 0, QModelIndex());
	scrollTo(index);
	clearSelection();
	selectRow(index.row());
	
	return true;
}

void
Table::SelectOneFile(const int index) {
	selectRow(index);
}

void
Table::SelectNextRow(const int next)
{
	int row_index = GetFirstSelectedRowIndex();
	int next_index = 0;
	if (row_index != -1)
		next_index = row_index + next;
	
	if (next_index < 0 || next_index >= table_model_->rowCount())
		return;
	
	selectRow(next_index);
}

void
Table::ShowRightClickMenu(const QPoint &pos)
{
	App *app = table_model_->app();
	QMenu *menu = new QMenu();
	
	{
		QAction *action = menu->addAction(tr("Create New Folder"));
		auto *icon = app->GetIcon(QLatin1String("special_folder"));
		if (icon != nullptr)
			action->setIcon(*icon);
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::CreateNewFolder);});
	}
	
	{
		QAction *action = menu->addAction(tr("Create New File"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::CreateNewFile);});
		auto *icon = app->GetIcon(QLatin1String("text"));
		if (icon != nullptr)
			action->setIcon(*icon);
	}
	
	{
		QAction *action = menu->addAction(tr("Delete Files"));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteFiles);});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
	}
	
	menu->popup(pos);
}

void
Table::SortingChanged(int logical, Qt::SortOrder order) {
	io::SortingOrder sorder = {Column(logical), order == Qt::AscendingOrder};
	io::Files *files = table_model_->files();
	int file_count;
	{
		MutexGuard guard(&files->mutex);
		file_count = files->vec.size();
		files->sorting_order = sorder;
		std::sort(files->vec.begin(), files->vec.end(), cornus::io::SortFiles);
	}
	//int rh = verticalHeader()->defaultSectionSize();
	//emit dataChanged(QModelIndex(), QModelIndex());
	table_model_->UpdateRowRange(0, file_count - 1);
}
} // cornus::gui::

