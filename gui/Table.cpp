#include "Table.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "AttrsDialog.hpp"
#include "../AutoDelete.hh"
#include "CountFolder.hpp"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "../Hid.hpp"
#include "OpenOrderPane.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "../io/socket.hh"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "RestorePainter.hpp"
#include "../str.hxx"
#include "Tab.hpp"
#include "TableDelegate.hpp"
#include "TableHeader.hpp"
#include "TableModel.hpp"

#include <map>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
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

Table::Table(TableModel *tm, App *app, Tab *tab) : app_(app),
model_(tm), tab_(tab)
{
	setSortingEnabled(true);
	setModel(model_);
	header_ = new TableHeader(this);
	setHorizontalHeader(header_);
// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	delegate_ = new TableDelegate(this, app_, tab_);
	setAlternatingRowColors(false);
	auto d = static_cast<QAbstractItemDelegate*>(delegate_);
	setItemDelegate(d);
	{
		auto *hz = horizontalHeader();
		hz->setSectionHidden(int(Column::TimeModified), true);
		hz->setSortIndicator(int(Column::FileName), Qt::AscendingOrder);
		connect(hz, &QHeaderView::sortIndicatorChanged, this, &Table::SortingChanged);
		
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
	//setShowGrid(false);
	setSelectionMode(QAbstractItemView::NoSelection);//ExtendedSelection);
	//setSelectionBehavior(QAbstractItemView::SelectRows);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	SetCustomResizePolicy();
	auto *vs = verticalScrollBar();
	connect(vs, &QAbstractSlider::valueChanged, this, &Table::HiliteFileUnderMouse);
}

Table::~Table()
{
	delete model_;
	delete delegate_;
}

void Table::ApplyPrefs()
{
	auto &map = app_->prefs().cols_visibility();
	auto *hz = horizontalHeader();
	QMap<i8, bool>::const_iterator it = map.constBegin();
	
	while (it != map.constEnd())
	{
		const bool hidden = !it.value();
		hz->setSectionHidden(it.key(), hidden);
		++it;
	}
}

void Table::AutoScroll(const VDirection d)
{
	cint rh = GetRowHeight();
	cint scroll = rh / 3;
	cint amount = (d == VDirection::Up) ? -scroll : scroll;
	auto *vs = verticalScrollBar();
	cint max = vs->maximum();
	int new_val = vs->value() + amount;
	
	if (new_val >= 0 && new_val <= max) {
		vs->setValue(new_val);
//		mtl_info("val: %d, new_val: %d, amount: %d, max: %d",
//			vs->value(), new_val, amount, max);
	}
}

bool Table::CheckIsOnFileName(io::File *file, cint file_row, const QPoint &pos) const
{
	if (!file->is_dir() || pos.y() < 0)
		return false;
	
	i32 col = columnAt(pos.x());
	if (col != (int)Column::FileName)
		return false;
	
	if (rowAt(pos.y()) != file_row)
		return false;
	
	QFontMetrics fm = fontMetrics();
	cint name_w = fm.horizontalAdvance(file->name());
	cint absolute_name_end = name_w + columnViewportPosition(col);
	
	return (pos.x() < absolute_name_end + FileNameRelax);
}

void Table::ClearDndAnimation(const QPoint &drop_coord)
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
		cint end = row + 1;
		model_->UpdateFileIndexRange(start, end);
	}
}

void Table::ClearMouseOver()
{
	mouse_pos_ = {-1, -1};
	mouse_over_file_name_ = -1;
	mouse_over_file_icon_ = -1;
}

void Table::dragEnterEvent(QDragEnterEvent *evt)
{
	tab_->DragEnterEvent(evt);
}

void Table::dragLeaveEvent(QDragLeaveEvent *evt)
{
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

void Table::dragMoveEvent(QDragMoveEvent *evt)
{
	drop_coord_ = evt->position().toPoint();
	ClearDndAnimation(drop_coord_);
	cint h = size().height() - horizontalHeader()->size().height();
	cint y = drop_coord_.y();
	cint rh = GetRowHeight();
	
	if (y >= (h - rh)) {
		AutoScroll(VDirection::Down);
	}
}

void Table::dropEvent(QDropEvent *evt)
{
	mouse_down_ = false;
	tab_->DropEvent(evt, ForceDropToCurrentDir::No);
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

io::File* Table::GetFileAt_NoLock(const QPoint &pos, const Clone clone, int *ret_file_index)
{
	int row = rowAt(pos.y());
	if (row == -1) {
		return nullptr;
	}
	
	io::Files &files = tab_->view_files();
	auto &vec = files.data.vec;
	if (row >= vec.size()) {
		return nullptr;
	}
	
	if (ret_file_index != nullptr) {
		*ret_file_index = row;
	}
	
	io::File *file = vec[row];

	return (clone == Clone::Yes) ? file->Clone() : file;
}

i32 Table::GetFileAt_NoLock(const QPoint &local_pos, const PickedBy pb, io::File **ret_file)
{
	const Column col = static_cast<Column>(columnAt(local_pos.x()));
	const Column curr = (pb == PickedBy::Icon) ? Column::Icon : Column::FileName;
	if (col != curr)
			return -1;
	
	io::File *file = GetFileAt_NoLock(local_pos, Clone::No);
	if (file == nullptr)
		return -1;
	
	if (pb == PickedBy::VisibleName)
	{
		QFontMetrics fm = fontMetrics();
		cint name_w = std::max(delegate_->min_name_w(), fm.horizontalAdvance(file->name()));
		cint absolute_name_end = name_w + columnViewportPosition((int)col);
		
		if (local_pos.x() > absolute_name_end + gui::FileNameRelax)
			return -1;
	}
	
	if (ret_file != nullptr)
		*ret_file = file;
	
	return rowAt(local_pos.y());
}

int Table::GetVisibleFileIndex()
{
	cint row = rowAt(0) + GetVisibleRowsCount() / 2;
	//mtl_info("row: %d, rh: %d", row, GetRowHeight());
	return row;
}

int Table::GetRowHeight() const { return verticalHeader()->defaultSectionSize(); }

i32 Table::GetVisibleRowsCount() const
{
	return (height() - horizontalHeader()->height()) / GetRowHeight();
}

void Table::HiliteFileUnderMouse()
{
	QSet<int> indices;
	i32 name_row = -1, icon_row = -1;
	{
		io::Files &files = tab_->view_files();
		MutexGuard guard = files.guard();
		name_row = GetFileAt_NoLock(mouse_pos_, PickedBy::VisibleName);
		if (name_row == -1)
			icon_row = GetFileAt_NoLock(mouse_pos_, PickedBy::Icon);
	}
	
	if (icon_row != mouse_over_file_icon_) {
		if (icon_row != -1)
			indices.insert(icon_row);
		if (mouse_over_file_icon_ != -1)
			indices.insert(mouse_over_file_icon_);
		mouse_over_file_icon_ = icon_row;
	} else if (name_row != mouse_over_file_name_) {
		if (name_row != -1)
			indices.insert(name_row);
		if (mouse_over_file_name_ != -1)
			indices.insert(mouse_over_file_name_);
		mouse_over_file_name_ = name_row;
	}
	
	model_->UpdateIndices(indices);
}

void Table::keyPressEvent(QKeyEvent *evt)
{
	tab_->KeyPressEvent(evt);
}

void Table::keyReleaseEvent(QKeyEvent *evt) {
	bool shift = evt->modifiers() & Qt::ShiftModifier;
	
	if (!shift) {
		shift_select_ = {};
	}
}

void Table::leaveEvent(QEvent *evt)
{
	const i32 row = (mouse_over_file_name_ == -1)
		? mouse_over_file_icon_ : mouse_over_file_name_;
	ClearMouseOver();
	model_->UpdateSingleRow(row);
}

void Table::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QTableView::mouseDoubleClickEvent(evt);
	i32 col = columnAt(evt->pos().x());
	auto *app = model_->app();
	
	if (evt->button() == Qt::LeftButton)
	{
		if (col == i32(Column::FileName))
		{
			io::Files &files = tab_->view_files();
			io::File *cloned_file = nullptr;
			{
				MutexGuard guard = files.guard();
				io::File *file = nullptr;
				cint row = GetFileAt_NoLock(evt->pos(), PickedBy::VisibleName, &file);
				Q_UNUSED(row);
				if (file != nullptr)
					cloned_file = file->Clone();
			}
			
			if (cloned_file)
				app->FileDoubleClicked(cloned_file, PickedBy::VisibleName);
		}
	}
}

void Table::mouseMoveEvent(QMouseEvent *evt)
{
	mouse_pos_ = evt->pos();
	HiliteFileUnderMouse();
	
	if (mouse_down_ && (drag_start_pos_.x() >= 0 || drag_start_pos_.y() >= 0)) {
		auto diff = (mouse_pos_ - drag_start_pos_).manhattanLength();
		if (diff >= QApplication::startDragDistance())
		{
			drag_start_pos_ = {-1, -1};
			tab_->StartDragOperation();
		}
	}
}

void Table::mousePressEvent(QMouseEvent *evt)
{
	QTableView::mousePressEvent(evt);
	mouse_down_ = true;
	
	const auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	const bool shift = modif & Qt::ShiftModifier;
	const bool right_click = evt->button() == Qt::RightButton;
	const bool left_click = evt->button() == Qt::LeftButton;
	QSet<int> indices;
	
	if (left_click) {
		if (ctrl) {
			app_->hid()->HandleMouseSelectionCtrl(tab_, evt->pos(), &indices);
		} else if (shift) {
			app_->hid()->HandleMouseSelectionShift(tab_, evt->pos(), indices);
		} else {
			app_->hid()->HandleMouseSelectionNoModif(tab_, evt->pos(),
				indices, true, &shift_select_);
		}
	}
	
	drag_start_pos_ = left_click ? evt->pos() : QPoint(-1, -1);
	
	if (right_click) {
		cauto pos = evt->position().toPoint();
		tab_->HandleMouseRightClickSelection(pos, indices);
		tab_->ShowRightClickMenu(evt->globalPosition().toPoint(), pos);
	}
	
	model_->UpdateIndices(indices);
}

void Table::mouseReleaseEvent(QMouseEvent *evt)
{
	QTableView::mouseReleaseEvent(evt);
	drag_start_pos_ = {-1, -1};
	mouse_down_ = false;
	
	QSet<int> indices;
	const bool ctrl = evt->modifiers() & Qt::ControlModifier;
	const bool shift = evt->modifiers() & Qt::ShiftModifier;
	
	if (!ctrl && !shift)
	{
		app_->hid()->HandleMouseSelectionNoModif(tab_,
			evt->pos(), indices, mouse_down_, &shift_select_);
	}
	
	model_->UpdateIndices(indices);
	
	if (evt->button() == Qt::LeftButton) {
		const i32 col = columnAt(evt->pos().x());
		if (col == i32(Column::Icon))
		{
			io::Files &files = tab_->view_files();
			io::File *cloned_file = nullptr;
			int file_index = -1;
			{
				auto g = files.guard();
				io::File *file = GetFileAt_NoLock(evt->pos(), Clone::No, &file_index);
				if (file != nullptr) {
					file->set_selected(true);
					cloned_file = file->Clone();
				}
			}
			if (cloned_file) {
				app_->hid()->SelectFileByIndex(tab_, file_index, DeselectOthers::Yes);
				app_->FileDoubleClicked(cloned_file, PickedBy::Icon);
			}
		}
	}
}

void Table::paintEvent(QPaintEvent *evt)
{
	QRect r(0, 0, viewport()->width(), viewport()->height());
	cbool magnify = tab_->magnified();
	if (magnify) {
		auto pe = QPaintEvent(r);
		QTableView::paintEvent(&pe);
	} else {
		QTableView::paintEvent(evt);
	}
	
	if (magnify)
		tab_->PaintMagnified(viewport(), view_options());
}

bool Table::ScrollToAndSelect(QString full_path)
{
	if (full_path.isEmpty()) {
		return false;
	}
	
	QString path;
	/// first remove trailing '/' or search will fail:
	if (full_path.endsWith('/'))
		path = full_path.mid(0, full_path.size() - 1);
	else
		path = full_path.mid(0, full_path.size());
	int row_count = -1;
	int row = -1;
	{
		io::Files &files = *app_->files(tab_->files_id());
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		row_count = vec.size();
		
		for (int i = 0; i < row_count; i++) {
			QString file_path = vec[i]->build_full_path();
			
			if (file_path == path) {
				row = i;
				break;
			}
		}
	}
	
	if (row == -1) {
		return false;
	}
	
	ScrollToFile(row);
	app_->hid()->SelectFileByIndex(tab_, row, DeselectOthers::Yes);
	shift_select_.base_row = row;
	return true;
}

void Table::ScrollToFile(int file_index)
{
	if (file_index < 0)
		return;
	
	cint rh = GetRowHeight();
	cint header_h = horizontalHeader()->height();
	int visible_rows = GetVisibleRowsCount();
	cint diff = height() % rh;
	if (diff > 0)
		visible_rows++;
	
	cint row_count = model_->rowCount();
	int max = (rh * row_count) - height() + header_h;
	if (max < 0) {
		max = 0;
		return; // no need to scroll
	}
	
	auto *vs = verticalScrollBar();
	vs->setMaximum(max);
	int row_at_pixel = rh * file_index;
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

void Table::SelectByLowerCase(QVector<QString> filenames,
	const NamesAreLowerCase are_lower)
{
	if (filenames.isEmpty())
		return;

	QSet<int> indices;
	QString full_path;
	io::Files &files = tab_->view_files();
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		cint count = vec.size();
		for (int i = 0; i < count; i++)
		{
			io::File *file = vec[i];
			if (filenames.isEmpty())
				break;
			
			const auto fn = (are_lower == NamesAreLowerCase::Yes)
				? file->name_lower() : file->name();
			
			cint found_at = filenames.indexOf(fn);
			if (found_at != -1)
			{
				if (full_path.isEmpty()) {
					full_path = file->build_full_path();
				}
				file->set_selected(true);
				indices.insert(i);
				filenames.remove(found_at);
			}
		}
	}
	
	model_->UpdateIndices(indices);
	ScrollToAndSelect(full_path);
}

void Table::SetCustomResizePolicy()
{
	auto *hh = horizontalHeader();
	hh->setSectionResizeMode(i8(gui::Column::Icon), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::FileName), QHeaderView::Stretch);
	hh->setSectionResizeMode(i8(gui::Column::Size), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::TimeCreated), QHeaderView::Fixed);
	hh->setSectionResizeMode(i8(gui::Column::TimeModified), QHeaderView::Fixed);
	QFontMetrics fm = fontMetrics();
	QString sample_date = QLatin1String("2020-12-01 18:04");
	
	cint icon_col_w = fm.horizontalAdvance(QLatin1String("Steam"));
	cint size_col_w = fm.horizontalAdvance(QLatin1String("1023.9 GiB")) + 2;
	cint time_col_w = fm.horizontalAdvance(sample_date) + 10;
	setColumnWidth(i8(gui::Column::Icon), icon_col_w);
	setColumnWidth(i8(gui::Column::Size), size_col_w);
	setColumnWidth(i8(gui::Column::TimeCreated), time_col_w);
	setColumnWidth(i8(gui::Column::TimeModified), time_col_w);
}

void Table::ShowVisibleColumnOptions(QPoint pos)
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

void Table::SortingChanged(int logical, Qt::SortOrder order)
{
	io::SortingOrder sorder = {Column(logical), order == Qt::AscendingOrder};
	io::Files &files = *app_->files(tab_->files_id());
	{
		MutexGuard guard = files.guard();
		files.data.sorting_order = sorder;
		std::sort(files.data.vec.begin(), files.data.vec.end(), cornus::io::SortFiles);
	}
	model_->UpdateVisibleArea();
}

void Table::SyncWith(const cornus::Clipboard &cl, QSet<int> &indices)
{
	auto &files = *app_->files(tab_->files_id());
	MutexGuard guard = files.guard();
	
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
//		mtl_info("Paste");
		flag = io::FileBits::ActionPaste;
	} else if (cl.action == ClipboardAction::Link) {
//		mtl_info("Link");
		flag = io::FileBits::PasteLink;
	}
	
	int i = -1;
	for (io::File *next: files.data.vec)
	{
		i++;
		bool added = false;
		if (next->clear_all_actions_if_needed()) {
			indices.insert(i);
			added = true;
		}
		
		cint count = file_paths.size();
		for (int k = 0; k < count; k++) {
			if (file_paths[k] == next->name()) {
				if (!added) {
					indices.insert(i);
				}
				next->toggle_flag(flag, true);
				file_paths.remove(k);
				break;
			}
		}
	}
}

void Table::UpdateLineHeight()
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

void Table::wheelEvent(QWheelEvent *evt)
{
	if (evt->modifiers() & Qt::ControlModifier)
	{
		auto y = evt->angleDelta().y();
		const Zoom zoom = (y > 0) ? Zoom::In : Zoom::Out;
		app_->prefs().WheelEventFromMainView(zoom);
		evt->ignore();
	} else {
		QTableView::wheelEvent(evt);
	}
}

} // cornus::gui::


