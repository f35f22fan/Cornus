#include "SidePane.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "SidePaneItem.hpp"
#include "SidePaneModel.hpp"
#include "TableDelegate.hpp"

#include <map>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
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

const QString BookmarkMime = QLatin1String("app/cornus_bookmark");

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
	
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	resizeColumnsToContents();
	//setShowGrid(false);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::NoSelection);//ExtendedSelection);
	{
		setDragEnabled(true);
		setAcceptDrops(true);
		setDragDropOverwriteMode(false);
		setDropIndicatorShown(true);
	}
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	
	int sz = GetIconSize();
	setIconSize(QSize(sz, sz));
}

SidePane::~SidePane() {
	delete model_;
}

void
SidePane::DeselectAllItems(const int except_row, const bool row_flag,
QVector<int> &indices)
{
	auto &items = this->items();
	MutexGuard guard(&items.mutex);
	
	int i = -1;
	for (SidePaneItem *next: items.vec)
	{
		i++;
		if (!next->is_bookmark())
			continue;
		
		if (i == except_row) {
			next->selected(row_flag);
			indices.append(i);
		} else {
			if (next->selected()) {
				next->selected(false);
				indices.append(i);
			}
		}
	}
}

void
SidePane::dragEnterEvent(QDragEnterEvent *event)
{
	const QMimeData *mimedata = event->mimeData();
	
	if (mimedata->hasUrls())
		event->acceptProposedAction();
	else if (!mimedata->data(BookmarkMime).isEmpty())
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
	
	if (row != -1) {
		int start = row;
		if (row > 0)
			start--;
		int end = row + 1;
		model_->UpdateRowRange(start, end);
	}
}

void
SidePane::dropEvent(QDropEvent *evt)
{
	drop_y_coord_ = -1;
	App *app = model_->app();
	
	if (evt->mimeData()->hasUrls()) {
		QVector<io::File*> *files_vec = new QVector<io::File*>();
		
		for (const QUrl &url: evt->mimeData()->urls())
		{
			io::File *file = io::FileFromPath(url.path());
			if (file != nullptr)
				files_vec->append(file);
		}
		
		int row;
		{
			SidePaneItems &items = app->side_pane_items();
			MutexGuard guard(&items.mutex);
			const int y = evt->pos().y();
			const int rh = verticalHeader()->defaultSectionSize();
			row = rowAt(y);
			if (y % rh >= (rh/2))
				row++;
			
			if (row == -1) {
				row = items.vec.size() - 1;
			}
		}
		
		model_->FinishDropOperation(files_vec, row);
		return;
	}
	
	
	QByteArray ba = evt->mimeData()->data(BookmarkMime);
	if (ba.isEmpty())
		return;
	
	QDataStream dataStreamRead(ba);
	QStringList str_list;
	dataStreamRead >> str_list;
	
	model_->MoveBookmarks(str_list, evt->pos());
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

int
SidePane::GetSelectedBookmarkCount() {
	gui::SidePaneItems &items = app_->side_pane_items();
	MutexGuard guard(&items.mutex);
	int count = 0;
	
	for (gui::SidePaneItem *next: items.vec) {
		if (next->selected() && next->is_bookmark())
			count++;
	}
	
	return count;
}

SidePaneItem*
SidePane::GetSelectedBookmark(int *index)
{
	SidePaneItems &items = this->items();
	MutexGuard guard(&items.mutex);
	int i = 0;
	for (SidePaneItem *next: items.vec)
	{
		if (next->selected() && next->is_bookmark()) {
			if (index != nullptr)
				*index = i;
			return next->Clone();
		}
		i++;
	}
	
	return nullptr;
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
SidePane::mouseMoveEvent(QMouseEvent *evt)
{
	if (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0)
	{
		StartDrag(evt->pos());
	}
}

void
SidePane::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	
	if (evt->button() == Qt::LeftButton) {
		drag_start_pos_ = evt->pos();
	} else {
		drag_start_pos_ = {-1, -1};
	}
	const auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	
	QVector<int> indices;
	int row = rowAt(evt->pos().y());
	
	if (ctrl) {
		SelectRowSimple(row, true);
	} else {
		DeselectAllItems(row, true, indices);
	}
	
	if (evt->button() == Qt::RightButton) {
		ShowRightClickMenu(evt->globalPos());
		model_->UpdateIndices(indices);
		return;
	}
	
	SidePaneItem *cloned_item = nullptr;
	{
		SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		cloned_item = GetItemAtNTS(evt->pos(), true, &row);
	}
	
	if (cloned_item == nullptr)
		return;
	
	if (cloned_item->is_partition() && !cloned_item->mounted()) {
		delete cloned_item;
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
	if (action == actions::RenameBookmark) {
		RenameSelectedBookmark();
	} else if (action == actions::DeleteBookmark) {
		model_->DeleteSelectedBookmarks();
	}
}

void
SidePane::RenameSelectedBookmark()
{
	int index;
	SidePaneItem *item = GetSelectedBookmark(&index);
	
	if (item == nullptr)
		return;
	
	cornus::AutoDelete ad(item);
	bool ok;
	QString text;
	{
		auto *dialog = new QInputDialog(this);
		dialog->setWindowTitle(tr("Rename Bookmark"));
		dialog->setInputMode(QInputDialog::TextInput);
		dialog->setLabelText(tr("Name:"));
		dialog->setTextValue(item->bookmark_name());
		dialog->resize(350, 100);
		ok = dialog->exec();
		if (!ok)
			return;
		text = dialog->textValue();
		if (text.isEmpty())
			return;
	}
	{
		auto &items = this->items();
		MutexGuard guard(&items.mutex);
		
		if (index < 0 || index >= items.vec.size())
			return;
		
		SidePaneItem *bookmark = items.vec[index];
		bookmark->bookmark_name(text);
	}
	model_->UpdateSingleRow(index);
	app_->SaveBookmarks();
}

void
SidePane::resizeEvent(QResizeEvent *event) {
	QTableView::resizeEvent(event);
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
			if (vec[i]->DisplayString() == name) {
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
SidePane::SelectProperPartition(const QString &full_path)
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
		
		if (!next->mounted())
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
SidePane::SelectRowSimple(const int row, const bool skip_update)
{
	CHECK_TRUE_VOID((row >= 0));
	SidePaneItems &items = app_->side_pane_items();
	bool update = false;
	{
		MutexGuard guard(&items.mutex);
		auto &vec = items.vec;
		
		if (row < vec.size()) {
			vec[row]->selected(true);
			update = true;
		}
	}
	
	if (!skip_update && update)
		model_->UpdateSingleRow(row);
}

void
SidePane::ShowRightClickMenu(const QPoint &pos)
{
	const int selected_bookmarks = GetSelectedBookmarkCount();
	if (menu_ == nullptr)
		menu_ = new QMenu(this);
	else
		menu_->clear();
	
	if (selected_bookmarks > 0) 
	{
		QAction *action = menu_->addAction(tr("&Delete"));
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteBookmark);});
		
		action = menu_->addAction(tr("&Rename.."));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::RenameBookmark);});
		action->setIcon(QIcon::fromTheme(QLatin1String("insert-text")));
	}
	
	menu_->popup(pos);
}

void
SidePane::StartDrag(const QPoint &pos)
{
	auto diff = (pos - drag_start_pos_).manhattanLength();
	if (diff < QApplication::startDragDistance())
		return;

	drag_start_pos_ = {-1, -1};
	QStringList str_list;
	
	{
		auto &items = this->items();
		MutexGuard guard(&items.mutex);
		
		for (SidePaneItem *next: items.vec) {
			if (next->is_bookmark() && next->selected()) {
				str_list.append(next->bookmark_name());
			}
		}
	}
	
	if (str_list.isEmpty())
		return;
	
	QMimeData *mimedata = new QMimeData();
	QByteArray ba;
	QDataStream dataStreamWrite(&ba, QIODevice::WriteOnly);
	dataStreamWrite << str_list;
	mimedata->setData(BookmarkMime, ba);
	
	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimedata);
	drag->exec(Qt::MoveAction);
}

} // cornus::gui::

