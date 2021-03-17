#include "SidePane.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/disks.hh"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "SidePaneItem.hpp"
#include "SidePaneModel.hpp"
#include "TableDelegate.hpp"

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

const QString BookmarkMime = QLatin1String("app/cornus_bookmark");

SidePane::SidePane(SidePaneModel *tm, App *app) :
model_(tm), app_(app)
{
	setModel(model_);
/// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	setAlternatingRowColors(true);
	auto *hz = horizontalHeader();
	hz->setSortIndicatorShown(false);
	hz->setSectionResizeMode(QHeaderView::Stretch);
	hz->setSectionsMovable(false);
	UpdateLineHeight();
	
	verticalHeader()->setSectionsMovable(false);
	
	setDefaultDropAction(Qt::MoveAction);
	setUpdatesEnabled(true);
	resizeColumnsToContents();
	//setShowGrid(false);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::NoSelection);
	{
		setDragEnabled(true);
		setAcceptDrops(true);
		setDragDropOverwriteMode(false);
		setDropIndicatorShown(true);
	}
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	
	int sz = GetIconSize();
	setIconSize(QSize(sz, sz));
	auto *vs = verticalScrollBar();
	connect(vs, &QAbstractSlider::valueChanged, this, &SidePane::HiliteFileUnderMouse);
}

SidePane::~SidePane() {
	delete model_;
}

void
SidePane::ClearDndAnimation(const QPoint &drop_coord)
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
SidePane::ClearHasBeenClicked(QString dev_path, QString error_msg)
{
	//mtl_printq2("Clear event: ", dev_path);
	int row = 0;
	SidePaneItems &items = app_->side_pane_items();
	{
		MutexGuard guard(&items.mutex);
		
		for (SidePaneItem *next: items.vec) {
			if (next->is_partition() && next->dev_path() == dev_path) {
				next->has_been_clicked(false);
				break;
			}
			row++;
		}
	}
	
	model_->UpdateSingleRow(row);
	
	if (!error_msg.isEmpty()) {
		QMessageBox::warning(app_, "Partition event", error_msg);
	}
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
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

void
SidePane::dragMoveEvent(QDragMoveEvent *event)
{
	drop_coord_ = event->pos();
	ClearDndAnimation(drop_coord_);
}

void
SidePane::dropEvent(QDropEvent *evt)
{
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
			
			const int rh = verticalHeader()->defaultSectionSize();
			row = rowAt(drop_coord_.y() + rh / 2);
			if (row == -1)
				row = items.vec.size();
		}
		
		model_->FinishDropOperation(files_vec, row);
	} else {
		QByteArray ba = evt->mimeData()->data(BookmarkMime);
		if (ba.isEmpty())
			return;
		
		QDataStream dataStreamRead(ba);
		QStringList str_list;
		dataStreamRead >> str_list;
		model_->MoveBookmarks(str_list, evt->pos());
	}
	
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
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
SidePane::GetRowHeight() const { return verticalHeader()->defaultSectionSize(); }

int
SidePane::GetSelectedBookmarkCount() {
	SidePaneItems &items = app_->side_pane_items();
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

void SidePane::HiliteFileUnderMouse()
{
	int row = -1;
	{
		io::Files &files = app_->view_files();
		MutexGuard guard(&files.mutex);
		row = rowAt(mouse_pos_.y());
	}
	
	bool repaint = false;
	i32 old_row = mouse_over_item_at_;
	if (row != mouse_over_item_at_) {
		repaint = true;
		mouse_over_item_at_ = row;
	}
	
	if (repaint) {
		QVector<int> rows = {old_row, mouse_over_item_at_};
		model_->UpdateIndices(rows);
	}
}

SidePaneItems &SidePane::items() const { return app_->side_pane_items(); }

void
SidePane::keyPressEvent(QKeyEvent *event)
{
//	const int key = event->key();
//	auto *app = model_->app();
//	const auto modifiers = event->modifiers();
//	const bool any_modifiers = (modifiers != Qt::NoModifier);
//	const bool shift = (modifiers & Qt::ShiftModifier);
//	QVector<int> indices;
	
//	model_->UpdateIndices(indices);
}

void
SidePane::leaveEvent(QEvent *evt)
{
	drop_coord_ = {-1, -1};
	int row = mouse_over_item_at_;
	mouse_over_item_at_ = -1;
	model_->UpdateSingleRow(row);
}

void
SidePane::MountPartition(SidePaneItem *partition)
{
	auto *arg = new io::disks::MountPartitionData();
	arg->app = app_;
	arg->partition = partition;
	pthread_t th;
	int status = pthread_create(&th, NULL,
		cornus::io::disks::MountPartitionTh, arg);
	if (status != 0)
		mtl_status(status);
}

void
SidePane::mouseMoveEvent(QMouseEvent *evt)
{
	mouse_pos_ = evt->pos();
	HiliteFileUnderMouse();
	
	if (mouse_down_ && (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0))
	{
		StartDrag(evt->pos());
	}
}

void
SidePane::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	mouse_down_ = true;
	
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
		model_->UpdateIndices(indices);
		ShowRightClickMenu(evt->globalPos(), evt->pos());
		return;
	}
	
	SidePaneItem *cloned_item = nullptr;
	{
		SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		auto *item = GetItemAtNTS(evt->pos(), false, &row);
		if (item != nullptr) {
			if (item->is_partition() && !item->mounted())
				item->has_been_clicked(true);
			cloned_item = item->Clone();
		}
	}
	
	if (cloned_item == nullptr)
		return;
	
	if (cloned_item->is_partition()) {/// && !cloned_item->has_been_clicked()) {
		if (!cloned_item->mounted()) {
			io::disks::MountPartitionData *mps = new io::disks::MountPartitionData();
			mps->app = app_;
			mps->partition = cloned_item;
			pthread_t th;
			int status = pthread_create(&th, NULL, io::disks::MountPartitionTh, mps);
			if (status != 0)
				mtl_status(status);
			return;
		}
	}
	
	if (!cloned_item->is_partition() || cloned_item->mounted())
		model_->app()->GoTo(Action::To, {cloned_item->mount_path(), Processed::No});
	model_->UpdateIndices(indices);
	delete cloned_item;
}

void
SidePane::mouseReleaseEvent(QMouseEvent *evt)
{
	QTableView::mouseReleaseEvent(evt);
	drag_start_pos_ = {-1, -1};
	mouse_down_ = false;
///	model_->UpdateIndices(indices);
}

void
SidePane::DrawBookmarksDropHere(QPainter &painter, const int at_index)
{
///	Qt::Alignment text_alignment_ = Qt::AlignLeft | Qt::AlignVCenter;
	const int rh = GetRowHeight();
	int y = rh * at_index;
	painter.setPen(option().palette.highlight().color());
	painter.drawText(2, y, tr("Drop"));
	painter.drawText(2, y += rh, tr("Bookmarks"));
	painter.drawText(2, y += rh, tr("Here"));
}

void
SidePane::paintEvent(QPaintEvent *evt)
{
	QTableView::paintEvent(evt);
	
	QPainter painter(viewport());
	{
		SidePaneItems &items = app_->side_pane_items();
		int count;
		bool has_bookmarks = false;
		{
			MutexGuard guard(&items.mutex);
			QVector<SidePaneItem*> &vec = items.vec;
			count = vec.size();
			for (int i = count - 1; i >= 0; i--) {
				if (vec[i]->is_bookmark()) {
					has_bookmarks = true;
					break;
				}
			}
		}
		if (!has_bookmarks) {
			DrawBookmarksDropHere(painter, count + 1);
		}
	}
	
	if (drop_coord_.y() == -1)
		return;
	
	const int rh = verticalHeader()->defaultSectionSize();
	const int count = model_->rowCount();
	int row = rowAt(drop_coord_.y() + rh / 2);
	int y = -1;
	
	if (row == count || row == -1) {
		y = height() - horizontalHeader()->height() - 4;
	} else {
		y = verticalHeader()->sectionViewportPosition(row);
	}
	
	QPen pen(QColor(0, 0, 255));
	pen.setWidthF(2.0);
	painter.setPen(pen);
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
SidePane::ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos)
{
	int row = 0;
	bool is_bookmark = false;
	bool is_mounted = false;
	{
		MutexGuard guard(&app_->side_pane_items().mutex);
		gui::SidePaneItem *cloned_item = GetItemAtNTS(local_pos, true, &row);
		
		if (cloned_item == nullptr)
			return;
		
		is_bookmark = cloned_item->is_bookmark();
		is_mounted = !is_bookmark && cloned_item->mounted();
	}
	
	if (menu_ == nullptr)
		menu_ = new QMenu(this);
	else
		menu_->clear();
	
	if (is_bookmark) 
	{
		QAction *action = menu_->addAction(tr("&Delete"));
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteBookmark);});
		
		action = menu_->addAction(tr("&Rename.."));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::RenameBookmark);});
		action->setIcon(QIcon::fromTheme(QLatin1String("insert-text")));
	} else {
		if (is_mounted) {
			QAction *action = menu_->addAction(tr("&Unmount"));
			action->setIcon(QIcon::fromTheme(QLatin1String("media-eject")));
			connect(action, &QAction::triggered, [=] () {
				UnmountPartition(row);
			});
		}
	}
	
	menu_->popup(global_pos);
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

void
SidePane::UnmountPartition(int row)
{
	SidePaneItem *partition = nullptr;
	auto &items = app_->side_pane_items();
	{
		MutexGuard guard(&items.mutex);
		SidePaneItem *item = items.vec[row];
		item->has_been_clicked(true);
		partition = item->Clone();
	}
	
	if (!partition->is_partition())
		return;
	
	auto *arg = new io::disks::MountPartitionData();
	arg->app = app_;
	arg->partition = partition;
	pthread_t th;
	int status = pthread_create(&th, NULL,
		cornus::io::disks::UnmountPartitionTh, arg);
	if (status != 0)
		mtl_status(status);
}

void
SidePane::UpdateLineHeight()
{
	auto *vh = verticalHeader();
	if (false) {
		auto fm = fontMetrics();
		int str_h = fm.height();
		int ln = str_h * 1.5;

		vh->setMinimumSectionSize(str_h);
		vh->setMaximumSectionSize(ln);
		vh->setDefaultSectionSize(ln);
	}
	vh->setSectionResizeMode(QHeaderView::Fixed);
	vh->setSectionsMovable(false);
}

void
SidePane::wheelEvent(QWheelEvent *evt)
{
	if (evt->modifiers() & Qt::ControlModifier)
	{
		auto y = evt->angleDelta().y();
		const Zoom zoom = (y > 0) ? Zoom::In : Zoom::Out;
		app_->prefs().AdjustCustomTableSize(zoom);
		evt->ignore();
	} else {
		QTableView::wheelEvent(evt);
	}
}

} // cornus::gui::

