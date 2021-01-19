#include "SidePane.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "SidePaneItem.hpp"
#include "SidePaneModel.hpp"
#include "TableDelegate.hpp"

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

SidePane::SidePane(SidePaneModel *tm, App *app) :
model_(tm), app_(app)
{
	setModel(model_);
	setAlternatingRowColors(true);
	auto *hz = horizontalHeader();
	hz->setSortIndicatorShown(false);
	horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	horizontalHeader()->setSectionsMovable(false);
	verticalHeader()->setSectionsMovable(false);
	setDragEnabled(true);
	setAcceptDrops(true);
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	setIconSize(QSize(32, 32));
	resizeColumnsToContents();
	//setShowGrid(false);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::NoSelection);//ExtendedSelection);
//	setDragDropOverwriteMode(false);
//	setDropIndicatorShown(true);
	
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

SidePane::~SidePane() {
	delete model_;
}

void SidePane::DeselectAllItems(const int except_row, const bool row_flag,
QVector<int> &indices) {
	auto &items = this->items();
	MutexGuard guard(&items.mutex);
	
	int i = 0;
	for (SidePaneItem *next: items.vec) {
		if (i == except_row) {
			next->selected(row_flag);
			indices.append(i);
		} else {
			if (next->selected()) {
				next->selected(false);
				indices.append(i);
			}
		}
		
		i++;
	}
}

void
SidePane::dragEnterEvent(QDragEnterEvent *event)
{
	const QMimeData *mimedata = event->mimeData();
	
	if (mimedata->hasUrls())
		event->acceptProposedAction();
}

void
SidePane::dragLeaveEvent(QDragLeaveEvent *event)
{
	drop_y_coord_ = -1;
	update();
}

void
SidePane::dragMoveEvent(QDragMoveEvent *event)
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
		model_->UpdateSingleRow(row);
}

void
SidePane::dropEvent(QDropEvent *event)
{
	drop_y_coord_ = -1;
	App *app = model_->app();
	
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

gui::SidePaneItem*
SidePane::GetItemAtNTS(const QPoint &pos, bool clone, int *ret_index)
{
	int row = rowAt(pos.y());
	if (row == -1)
		return nullptr;
	
	SidePaneItems &items = app_->side_pane_items();
	auto &vec = items.vec;
	if (row >= vec.size())
		return nullptr;
	
	if (ret_index != nullptr)
		*ret_index = row;
	
	if (clone)
		return vec[row]->Clone();
	return vec[row];
}

gui::SidePaneItems&
SidePane::items() const { return app_->side_pane_items(); }

void
SidePane::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	auto *app = model_->app();
	const auto modifiers = event->modifiers();
	const bool any_modifiers = (modifiers != Qt::NoModifier);
	const bool shift = (modifiers & Qt::ShiftModifier);
	QVector<int> indices;
	
	model_->UpdateIndices(indices);
}

void
SidePane::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTableView::mouseDoubleClickEvent(evt);
	
	i32 col = columnAt(evt->pos().x());
	auto *app = model_->app();
	
	if (evt->button() == Qt::LeftButton) {
		
	}
}

void
SidePane::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	QVector<int> indices;
	int row = rowAt(evt->pos().y());
	DeselectAllItems(row, true, indices);
	
	auto modif = evt->modifiers();
	const bool ctrl_pressed = modif & Qt::ControlModifier;
	
	SidePaneItem *cloned_item = nullptr;
	{
		SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		cloned_item = GetItemAtNTS(evt->pos(), true, &row);
		
		if (cloned_item == nullptr)
			return;
	}
	
	model_->app()->GoTo(cloned_item->mount_path());
	
	
	model_->UpdateIndices(indices);
}

void
SidePane::paintEvent(QPaintEvent *event)
{
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
	
	int y = drop_y_coord_;
	
	int rem = y % row_h;
	
	if (rem < row_h / 2)
		y -= rem;
	else
		y += row_h - rem;
	
	if (y > 0)
		y -= 1;
	
	//mtl_info("y: %d, slider: %d", y, slider_pos);
	painter.drawLine(0, y, width(), y);
}

void
SidePane::ProcessAction(const QString &action)
{
	mtl_trace("TBD");
}

void
SidePane::resizeEvent(QResizeEvent *event) {
	QTableView::resizeEvent(event);
//	double w = event->size().width();
//	const int icon = 50;
//	const int size = 110;
//	QFontMetrics metrics = fontMetrics();
//	QString sample_date = QLatin1String("2020-12-01 18:04");
//	const int time_w = metrics.boundingRect(sample_date).width() * 1.1;
//	int file_name = w - (icon + size + time_w + 5);
	
//	setColumnWidth(i8(gui::Column::Icon), icon);// 45);
//	setColumnWidth(i8(gui::Column::FileName), file_name);// 500);
//	setColumnWidth(i8(gui::Column::Size), size);
//	setColumnWidth(i8(gui::Column::TimeCreated), time_w);
}

bool
SidePane::ScrollToAndSelect(QString name)
{
	int row = -1;
	{
		SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		auto &vec = items.vec;
		
		for (int i = 0; i < vec.size(); i++) {
			if (vec[i]->table_name() == name) {
				row = i;
				break;
			}
		}
	}
	
	if (row == -1)
		return false;
	
	QModelIndex index = model()->index(row, 0, QModelIndex());
	scrollTo(index);
	SelectRowSimple(row);
	
	return true;
}

void
SidePane::SelectItemByFilePath(const QString &full_path)
{
	SidePaneItems &items = app_->side_pane_items();
	MutexGuard guard(&items.mutex);
	auto &vec = items.vec;
	SidePaneItem *root_item = nullptr;
	int root_index = -1;
	bool found = false;
	QVector<int> indices;
	int i = -1;
	for (SidePaneItem *next: vec)
	{
		i++;
		if (!next->is_partition())
			continue;
		
		if (root_item == nullptr) {
			if (next->mount_path() == QLatin1String("/")) {
				root_item = next;
				root_index = i;
			}
		}
		
		if (next == root_item)
			continue;
		
		if (full_path.startsWith(next->mount_path())) {
			if (!next->selected()) {
				indices.append(i);
				next->selected(true);
			}
			found = true;
		} else {
			if (next->selected()) {
				next->selected(false);
				indices.append(i);
			}
		}
	}
	
	if (root_item != nullptr) {
		bool do_select = !found;
		if (root_item->selected() != do_select) {
			root_item->selected(do_select);
			indices.append(root_index);
		}
	}
	
	model_->UpdateIndices(indices);
}

void
SidePane::SelectRowSimple(const int row) {
	SidePaneItems &items = app_->side_pane_items();
	bool update = false;
	{
		MutexGuard guard(&items.mutex);
		auto &vec = items.vec;
		
		if (row < vec.size()) {
			//vec[row]->selected(true);
			mtl_trace("select(true) TBD");
			update = true;
		}
	}
	
	if (update)
		model_->UpdateSingleRow(row);
}

void
SidePane::ShowRightClickMenu(const QPoint &pos)
{
	App *app = model_->app();
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
	
	menu->popup(pos);
}

} // cornus::gui::

