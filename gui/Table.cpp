#include "Table.hpp"

#include "../App.hpp"
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
#include <QGuiApplication>
#include <QHeaderView>
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
	setSelectionBehavior(QAbstractItemView::SelectRows);
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
//	setSelectionMode(QAbstractItemView::ExtendedSelection);
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

void
Table::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	
//	if (key == Qt::Key_Delete) {
//		table_model_->app()->RemoveSongsFromPlaylist(Which::Selected);
//	}
}

void
Table::mouseDoubleClickEvent(QMouseEvent *event)
{
	QItemSelectionModel *select = selectionModel();
	
	if (!select->hasSelection())
		return;
	
	QModelIndexList rows = select->selectedRows();
	
	if (rows.isEmpty())
		return;
	
	i32 col = columnAt(event->pos().x());
	auto *app = table_model_->app();
	
	if (col == i32(Column::Icon)) {
		const int row_index = rows[0].row();
		io::File *file = nullptr;
		{
			io::Files *files = table_model_->files();
			MutexGuard guard(&files->mutex);
			auto &vec = files->vec;
			
			if (row_index >= vec.size())
				return;
			
			file = vec[row_index]->Clone();
		}
		app->FileDoubleClicked(file, Column::Icon);
	} else if (col == i32(Column::FileName)) {
		const int row_index = rows[0].row();
		io::File *file = nullptr;
		{
			io::Files *files = table_model_->files();
			MutexGuard guard(&files->mutex);
			auto &vec = files->vec;
			
			if (row_index >= vec.size())
				return;
			
			file = vec[row_index]->Clone();
		}
		app->FileDoubleClicked(file, Column::FileName);
	}
		
	
}

void
Table::mousePressEvent(QMouseEvent *event)
{
	QTableView::mousePressEvent(event);
	
	if (event->button() == Qt::RightButton) {
		//ShowRightClickMenu(event->globalPos());
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
	CHECK_PTR_VOID(table_model_);
	/**
	QItemSelectionModel *select = selectionModel();
	
	if (select->hasSelection()) {
		QModelIndexList rows = select->selectedRows();
		
		if (action == quince::actions::RemoveSongsAndDeleteFiles) {
			RemoveSongsAndDeleteFiles(rows);
		} else if (action == actions::ShowSongFolderPath) {
			if (rows.isEmpty())
				return;
			
			const int row_index = rows[0].row();
			QVector<Song*> &songs = table_model_->songs();
			
			if (row_index >= songs.size())
				return;
			
			ShowSongLocation(songs[row_index]);
		}
	} **/
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
	int icon = 50;
	int size = 120;
	int file_name = w - (icon + size + 5);
	
	setColumnWidth(i8(gui::Column::Icon), icon);// 45);
	setColumnWidth(i8(gui::Column::FileName), file_name);// 500);
	setColumnWidth(i8(gui::Column::Size), size);//120);
}

void
Table::ScrollToAndSelect(const QString &full_path)
{
	int row = -1;
	{
		auto *files = table_model_->files();
		MutexGuard guard(&files->mutex);
		auto &vec = files->vec;
		
		
		for (int i = 0; i < vec.size(); i++) {
			auto *file = vec[i];
			if (file->build_full_path() == full_path) {
				row = i;
				break;
			}
		}
	}
	
	if (row == -1)
		return;
	
	QModelIndex index = model()->index(row, 0, QModelIndex());
	scrollTo(index);
	selectRow(index.row());
}

void
Table::ShowRightClickMenu(const QPoint &pos)
{
	/*
	QMenu *menu = new QMenu();
	{
		auto action_str = quince::actions::RemoveSongsAndDeleteFiles;
		QAction *action = menu->addAction(action_str);
		connect(action, &QAction::triggered, [=] {ProcessAction(action_str);});
	}
	{
		auto action_str = quince::actions::ShowSongFolderPath;
		QAction *action = menu->addAction(action_str);
		connect(action, &QAction::triggered, [=] {ProcessAction(action_str);});
	}
	
	menu->popup(pos);
	*/
}

} // cornus::gui::

