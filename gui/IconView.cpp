#include "IconView.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../Hid.hpp"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "../Prefs.hpp"
#include "RestorePainter.hpp"
#include "Tab.hpp"
#include "Table.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImageReader>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace cornus::gui {

static ThumbLoaderArgs* ThumbLoaderArgsFromFile(Tab *tab,
	io::File *file, const DirId dir_id,
	const int max_img_w, const int max_img_h)
{
	MTL_CHECK_ARG(file != nullptr, nullptr);
	ThumbLoaderArgs *p = new ThumbLoaderArgs();
	p->app = tab->app();
	p->ba = file->thumbnail_attrs();
	p->full_path = file->build_full_path();
	p->file_id = file->id();
	p->ext = file->cache().ext.toLocal8Bit();
	p->tab_id = tab->id();
	p->dir_id = dir_id;
	p->icon_w = max_img_w;
	p->icon_h = max_img_h;
	
	return p;
}

IconView::IconView(App *app, Tab *tab, QScrollBar *vs):
	app_(app), tab_(tab), vs_(vs)
{
	Q_UNUSED(app_);
	Q_UNUSED(zoom_);
	Init();
}

IconView::~IconView()
{}

int IconView::CellIndexInNextRow(const int file_index, const VDirection vdir)
{
	const int file_count = tab_->view_files().cached_files_count;
	
	if (file_index == -1)
		return (vdir == VDirection::Up) ? 0 : file_count - 1;
	
	const IconDim &cell = icon_dim_;
	int next;
	if (vdir == VDirection::Up)
	{
		next = file_index - cell.per_row;
		if (next < 0)
			next = file_index;
	} else {
		next = file_index + cell.per_row;
		if (next >= file_count)
			next = file_index;
	}
	
	return next;
}

void IconView::ClearDndAnimation(const QPoint &drop_coord)
{
	// repaint() or update() don't work because
	// the window is not raised when dragging an item
	// on top of the table and the repaint
	// requests are ignored. Thus repaint using a hack:
	const int row = GetRowAtY(drop_coord.y());
	
	if (row != -1) {
		int start = row;
		if (row > 0)
			start--;
		const int end = row + 1;
		UpdateFileIndexRange(start, end);
	}
}

void IconView::ClearMouseOver()
{
	mouse_pos_ = {-1, -1};
	mouse_over_file_ = -1;
}

void IconView::ComputeProportions(IconDim &dim) const
{
	const QString sample_str = QLatin1String("m");
	const QFontMetrics fm = fontMetrics();
	const QRect br = fm.boundingRect(sample_str);
	const double area_w = this->width();
	const double w = br.width() * 7;
	
	dim.gap = w / 16;
	dim.two_gaps = dim.gap * 2;
	dim.w = w;
	dim.w_and_gaps = dim.w + dim.two_gaps;
	dim.cell_and_gap = dim.w_and_gaps + dim.gap;
	dim.h = dim.w * 1.3;
	dim.h_and_gaps = dim.h + dim.two_gaps;
	dim.rh = dim.h_and_gaps + dim.gap;
	dim.text_rows = 3;
	dim.str_h = br.height();
	dim.text_h = dim.str_h * dim.text_rows;
	dim.text_y = dim.h_and_gaps - dim.str_h * dim.text_rows;
	dim.per_row = std::max(1, (int)(area_w + dim.gap) / (int)dim.cell_and_gap);
	
	const int file_count = tab_->view_files().cached_files_count;
	dim.row_count = file_count / dim.per_row;
	if (file_count % dim.per_row)
		dim.row_count++;
	
	dim.total_h = dim.rh * dim.row_count;
}

void IconView::DelayedRepaint()
{
	const i64 last_repaint_ms = last_repaint_.elapsed_ms();
	const i64 remaining_ms = delay_repaint_ms_ - last_repaint_ms;
	
	if (remaining_ms > 0) {
		const auto ms = (remaining_ms >= delay_repaint_ms_ / 2) ?
			remaining_ms : delay_repaint_ms_;
		delayed_repaint_pending_ = true;
		QTimer::singleShot(ms, this, &IconView::DelayedRepaint);
	} else {
		update();
		delayed_repaint_pending_ = false;
	}
}

void IconView::dragEnterEvent(QDragEnterEvent *evt)
{
	tab_->DragEnterEvent(evt);
}

void IconView::dragLeaveEvent(QDragLeaveEvent *evt)
{
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

void IconView::dragMoveEvent(QDragMoveEvent *evt)
{
	drop_coord_ = evt->pos();
	ClearDndAnimation(drop_coord_);
	int h = size().height();
	const int y = drop_coord_.y();
	const int rh = icon_dim_.rh;
	
	if (y >= (h - rh))
	{
		mtl_warn("this logic doesn't work with IconView");
		ScrollByWheel(VDirection::Down, ScrollBy::LineStep);
	}
}

void IconView::dropEvent(QDropEvent *evt)
{
	mouse_down_ = false;
	tab_->DropEvent(evt, ForceDropToCurrentDir::No);
	ClearDndAnimation(drop_coord_);
	drop_coord_ = {-1, -1};
}

DrawBorder IconView::DrawThumbnail(io::File *file, QPainter &painter,
	double x, double y)
{
	const auto &cell = icon_dim_;
	const int max_img_h = cell.h_and_gaps - cell.text_h;
	const int max_img_w = cell.w_and_gaps;
	QPixmap pixmap;
	
	if (file->thumbnail() != nullptr) {
		pixmap = QPixmap::fromImage(file->thumbnail()->img);
	} else {
		QIcon *icon = app_->GetFileIcon(file);
		pixmap = icon->pixmap(128, 128);
	}
	const double pw = pixmap.width();
	const double ph = pixmap.height();
	double used_w, used_h;
	if (int(pw) > max_img_w || int(ph) > max_img_h)
	{ // this happens when it's not a thumbnail but just a file icon.
		if (pw > max_img_w || ph > max_img_h)
		{
			double w_ratio = pw / max_img_w;
			double h_ratio = ph / max_img_h;
			const double ratio = std::max(w_ratio, h_ratio);
			used_w = pw / ratio;
			used_h = ph / ratio;
		} else {
			used_w = pw;
			used_h = ph;
		}
	} else {
		used_w = pw;
		used_h = ph;
	}
	
	double img_x = x + (max_img_w - used_w) / 2;
	painter.drawPixmap(img_x, y, used_w, used_h, pixmap);
	
	auto area_img = used_w * used_h;
	auto area_avail = max_img_w * max_img_h;
	const auto area_img_ratio = area_avail / area_img;
	return (area_img_ratio > 1.45) ? DrawBorder::Yes : DrawBorder::No;
}

void IconView::DisplayingNewDirectory(const DirId dir_id, const Reload r)
{
	if (last_thumbnail_submission_for_ == dir_id)
		return;
	
	if (last_cancelled_except_ != dir_id)
	{
		last_cancelled_except_ = dir_id;
		app_->RemoveAllThumbTasksExcept(dir_id);
	}
	
	if (is_current_view())
	{
		SendLoadingNewThumbnailsBatch();
		UpdateScrollRange();
		if (r == Reload::No)
			vs_->setValue(0);
	}
}

void IconView::FileChanged(const io::FileEventType evt, io::File *cloned_file)
{
	//auto &id = cloned_file->id();
	//mtl_info("File ID: %lu, %u, %u", id.inode_number, id.dev_major, id.dev_minor);
	
	if (cloned_file != nullptr)
	{
		SendLoadingNewThumbnail(cloned_file);
		delete cloned_file;
	}
	
	if (is_current_view())
	{
		const bool file_count_changed = (evt == io::FileEventType::Created)
			|| (evt == io::FileEventType::Deleted);
		if (file_count_changed)
		{
			UpdateScrollRange();
		}
		RepaintLater();
	}
}

void IconView::HiliteFileUnderMouse()
{
	QSet<int> indices;
	if (mouse_over_file_ != -1)
		indices.insert(mouse_over_file_);
	
	{
		io::Files &files = tab_->view_files();
		MutexGuard guard = files.guard();
		GetFileAt_NoLock(mouse_pos_, Clone::No, &mouse_over_file_);
	}
	
	if (mouse_over_file_ != -1 && !indices.contains(mouse_over_file_))
		indices.insert(mouse_over_file_);
	
	UpdateIndices(indices);
}

void IconView::Init()
{
	setFocusPolicy(Qt::WheelFocus);
	
	// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	{
		//setDragEnabled(true);
		setAcceptDrops(true);
//		setDragDropOverwriteMode(false);
//		setDropIndicatorShown(true);
//		setDefaultDropAction(Qt::MoveAction);
	}
	connect(vs_, &QScrollBar::valueChanged, [=](int value) {
		if (repaint_without_delay_)
			update();
		else
			update();//RepaintLater();
	});
	
	connect(vs_, &QScrollBar::sliderReleased, [=] {
		update();
	});
}

bool IconView::is_current_view() const
{
	return tab_->view_mode() == ViewMode::Icons;
}

i32 IconView::GetFileIndexAt(const QPoint &pos) const
{
	const auto &cell = icon_dim_;
	const int mouse_y = pos.y() + vs_->value();
	const int row_index = std::max(0, GetRowAtY(mouse_y));
	const int x = pos.x();
	const int max_real_x = cell.per_row * cell.cell_and_gap;
	const int x_index = (x > max_real_x) ? -1 : x / cell.cell_and_gap;
	
	const int file_index = (x_index == -1) ? -1 : (row_index * cell.per_row + x_index);
	const auto &files = tab_->view_files();
	const int file_count = files.cached_files_count;
	if (file_index < 0 || file_index >= file_count)
	{
		return -1;
	}
	
	return file_index;
}

io::File* IconView::GetFileAt_NoLock(const QPoint &pos, const Clone c, int *ret_index)
{
	const int file_index = GetFileIndexAt(pos);
	if (ret_index)
		*ret_index = file_index;
	
	if (file_index == -1)
		return nullptr;
	
	auto &files = tab_->view_files();
	io::File *f = files.data.vec[file_index];
	
	return (c == Clone::Yes) ? f->Clone() : f;
}

i32 IconView::GetVisibleFileIndex()
{
	return GetFileIndexAt(QPoint(width() / 2, height() / 2));
}

void IconView::keyPressEvent(QKeyEvent *evt)
{
	tab_->KeyPressEvent(evt);
}

void IconView::keyReleaseEvent(QKeyEvent *evt)
{
	bool shift = evt->modifiers() & Qt::ShiftModifier;
	Q_UNUSED(shift);
}

void IconView::leaveEvent(QEvent *evt)
{
	ClearMouseOver();
	UpdateIndex(mouse_over_file_);
}

void IconView::mouseDoubleClickEvent(QMouseEvent *evt)
{
	int row;
	io::File *file = GetFileAt_NoLock(evt->pos(), Clone::Yes, &row);
	if (file) {
		app_->FileDoubleClicked(file, PickBy::VisibleName);
		UpdateIndex(row);
	}
}

void IconView::mouseMoveEvent(QMouseEvent *evt)
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

void IconView::mousePressEvent(QMouseEvent *evt)
{
	mouse_down_ = true;
	
	const auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	const bool shift = modif & Qt::ShiftModifier;
	const bool right_click = evt->button() == Qt::RightButton;
	const bool left_click = evt->button() == Qt::LeftButton;
	QSet<int> indices;
	
	if (left_click) {
		drag_start_pos_ = evt->pos();
	} else {
		drag_start_pos_ = {-1, -1};
	}
	
	if (left_click)
	{
		if (ctrl) {
			app_->hid()->HandleMouseSelectionCtrl(tab_, evt->pos(), &indices);
		} else if (shift) {
			app_->hid()->HandleMouseSelectionShift(tab_, evt->pos(), indices);
		} else {
			app_->hid()->HandleMouseSelectionNoModif(tab_, evt->pos(),
				indices, true, &shift_select_);
		}
	}
	
	if (right_click)
	{
		tab_->HandleMouseRightClickSelection(evt->pos(), indices);
		tab_->ShowRightClickMenu(evt->globalPos(), evt->pos());
	}
	
	UpdateIndices(indices);
}

void IconView::mouseReleaseEvent(QMouseEvent *evt)
{
	drag_start_pos_ = {-1, -1};
	mouse_down_ = false;
	
	QSet<int> indices;
	const bool ctrl = evt->modifiers() & Qt::ControlModifier;
	const bool shift = evt->modifiers() & Qt::ShiftModifier;
	
	if (!ctrl && !shift)
	{
		app_->hid()->HandleMouseSelectionNoModif(tab_,
			evt->pos(), indices, false, &shift_select_);
	}
	
	UpdateIndices(indices);
}

void IconView::paintEvent(QPaintEvent *ev)
{
	last_repaint_.Continue(Reset::Yes);
	
	static bool first_time = true;
	if (first_time)
	{
		first_time = false;
		UpdateScrollRange();
	}
	
	QStyleOptionViewItem option = tab_->table()->view_options();
	QPainter painter(this);
	RestorePainter rp(&painter);
	painter.setFont(font());
	auto clear_r = QRect(0, 0, width(), height());
	painter.fillRect(clear_r, option.palette.brush(QPalette::Base));
	ComputeProportions(icon_dim_);
	const double scroll_y = vs_->value();
	const auto &cell = icon_dim_; // to make sure I don't change any value
	const int at_row = scroll_y / cell.rh;
	double remainder = std::fmod(scroll_y, cell.rh);
	{// std::numeric_limits<double>::epsilon() doesn't work here!
		const double my_epsilon = cell.rh / 1000000.0;
		if (cornus::DoublesEqual(remainder, cell.rh, my_epsilon)) {
			//mtl_info("%f vs %f", std::numeric_limits<double>::epsilon(), my_epsilon);
			remainder = 0;
		}
	}
	const double y_off = -remainder;
	const double y_end = remainder + height();
	const double width = std::max((double)this->width(), cell.cell_and_gap);
	int file_index = at_row * cell.per_row;
	QTextOption text_options;
	text_options.setAlignment(Qt::AlignHCenter);
	text_options.setWrapMode(QTextOption::WrapAnywhere);
	const QColor gray_color(100, 100, 100);
	io::Files &files = tab_->view_files();
	auto guard = files.guard();
	
	for (double y = y_off; y < y_end; y += cell.rh)
	{
		if (file_index >= files.cached_files_count)
			break;
		
		for (double x = 0.0; (x + cell.w_and_gaps) < width; x += cell.cell_and_gap)
		{
			if (file_index >= files.cached_files_count)
				break;
			const bool mouse_over = (mouse_over_file_ == file_index);
			QString file_name;
			const QRect cell_r(x, y, cell.w_and_gaps, cell.h_and_gaps);
			DrawBorder draw_border = DrawBorder::No;
			QString img_wh_str;
			{
				// used to lock files here
				io::File *file = files.data.vec[file_index];
				file_index++;
				file_name = file->name();
				if (mouse_over || file->is_selected())
				{
					QColor c = option.palette.highlight().color();
					if (mouse_over && !file->is_selected()) {
						c = app_->hover_bg_color_gray(c);
					}
					
					painter.fillRect(cell_r, c);
				}
				
				draw_border = DrawThumbnail(file, painter, x, y);
				const Thumbnail *thmb = file->thumbnail();
				if (thmb != nullptr)
				{
					img_wh_str = thumbnail::SizeToString(thmb->original_image_w,
						thmb->original_image_h, ViewMode::Icons);
				}
			}
			
			if (!img_wh_str.isEmpty())
			{
				QPen saved_pen = painter.pen();
				QBrush brush = option.palette.brush(QPalette::PlaceholderText);
				QPen pen(brush.color());
				painter.setPen(pen);
				const float text_y = y + cell.text_y;
				QRectF text_space(x, text_y, cell.w_and_gaps, cell.str_h);
				painter.drawText(text_space, img_wh_str, text_options);
				painter.setPen(saved_pen);
			}
			
			float text_y = y + cell.text_y;
			auto text_h = cell.text_h;
			if (!img_wh_str.isEmpty())
			{
				text_y += cell.str_h;
				text_h -= cell.str_h;
			}
			QRectF text_space(x, text_y, cell.w_and_gaps, text_h);
			painter.drawText(text_space, file_name, text_options);
			
			if (draw_border == DrawBorder::Yes)
			{
				QPen pen = painter.pen();
				QColor c = app_->hover_bg_color_gray(gray_color);
				painter.setPen(c);
				painter.drawRect(cell_r);
				painter.setPen(pen);
			}
		}
	}
}

void IconView::RepaintLater(const int custom_ms)
{
	if (!delayed_repaint_pending_)
	{
		delayed_repaint_pending_ = true;
		const auto ms = (custom_ms != -1) ? custom_ms : delay_repaint_ms_;
		QTimer::singleShot(ms, this, &IconView::DelayedRepaint);
	}
}

void IconView::resizeEvent(QResizeEvent *ev)
{
	UpdateScrollRange();
	update();
}

void IconView::ScrollByWheel(const VDirection d, const gui::ScrollBy sb)
{
	const int step = (sb == ScrollBy::LineStep)
		? icon_dim_.rh/2 : scroll_page_step_;
	const auto val = vs_->value();
	
	if (d == VDirection::Down)
	{
		vs_->setValue(val + step);
	} else {
		vs_->setValue(std::max(0, val - step));
	}
}

void IconView::ScrollToAndSelect(const int file_index, const DeselectOthers des)
{
	ScrollToFile(file_index);
	app_->hid()->SelectFileByIndex(tab_, file_index, des);
}

void IconView::ScrollToFile(const int file_index)
{
//	mtl_info("File index: %d, file_count: %d",
//		file_index, tab_->view_files().cached_files_count);
	if (icon_dim_.per_row <= 0)
		ComputeProportions(icon_dim_);
	
	const IconDim &cell = icon_dim_;
	const double curr_y = vs_->value();
	const int file_row = file_index / cell.per_row;
	const double file_y = double(file_row) * cell.rh;
	const double view_h = height();
	
	const bool is_fully_visible = (file_y >= curr_y) &&
		(file_y <= curr_y + view_h - cell.rh);
	
	if (is_fully_visible) {
//		static int k = 0;
//		mtl_info("Fully visible %d", k++);
		return;
	}
	
	int new_val = -1;
	if (file_y < curr_y) {
		new_val = file_y;
//		mtl_info("File was above: %f, new_row: %d, rh: %f",
//			file_y, file_row, cell.rh);
	} else {
		new_val = file_y - (view_h - cell.rh);
//		mtl_info("File was below: %f, new_row: %d, rh: %f",
//			file_y, file_row, cell.rh);
	}
	
	if (new_val >= 0) {
		vs_->setValue(new_val);
	}
}

void IconView::SendLoadingNewThumbnailsBatch()
{
	auto &files = tab_->view_files();
	MTL_CHECK_VOID(files.Lock());
	
	const DirId dir_id = files.data.dir_id;
	files.Unlock();
	
	if (dir_id == last_thumbnail_submission_for_)
		return;
	
	last_thumbnail_submission_for_ = dir_id;
	ComputeProportions(icon_dim_);
	const auto &cell = icon_dim_; // to make sure I don't change any value
	const int max_img_h = cell.h_and_gaps - cell.text_h;
	const int max_img_w = cell.w_and_gaps;
	
	QVector<ThumbLoaderArgs*> *work_stack = new QVector<ThumbLoaderArgs*>();
	{
		auto g = files.guard();
		for (io::File *file: files.data.vec)
		{
			if (!file->ShouldTryLoadingThumbnail())
				continue;
			
			file->cache().tried_loading_thumbnail = true;
			auto *arg = ThumbLoaderArgsFromFile(tab_, file, dir_id,
				max_img_w, max_img_h);
			work_stack->append(arg);
		}
	}
	
	app_->SubmitThumbLoaderBatchFromTab(work_stack, tab_->id(), dir_id);
}

void IconView::SendLoadingNewThumbnail(io::File *cloned_file)
{
	if (cloned_file == nullptr || !cloned_file->ShouldTryLoadingThumbnail())
	{
		if (cloned_file == nullptr)
			mtl_trace();
		return;
	}
	
	auto &files = tab_->view_files();
	MTL_CHECK_VOID(files.Lock());
	const DirId dir_id = files.data.dir_id;
	files.Unlock();
	
	//ComputeProportions(icon_dim_);
	const auto &cell = icon_dim_; // to make sure I don't change any value
	const int max_img_h = cell.h_and_gaps - cell.text_h;
	const int max_img_w = cell.w_and_gaps;
	
	cloned_file->cache().tried_loading_thumbnail = true;
	auto *arg = ThumbLoaderArgsFromFile(tab_, cloned_file, dir_id,
		max_img_w, max_img_h);
	
	app_->SubmitThumbLoaderFromTab(arg);
}

void IconView::SetAsCurrentView(const NewState ns)
{
	if (ns == NewState::AboutToSet)
	{
		UpdateScrollRange();
		SendLoadingNewThumbnailsBatch();
	} else if (ns == NewState::Set) {
		//SendLoadingNewThumbnailsBatch();
	}
}

QSize IconView::size() const
{
	ComputeProportions(icon_dim_);
	int w = parentWidget()->size().width();
	return QSize(w, icon_dim_.total_h);
}

void IconView::UpdateIndex(const int file_index)
{
	auto &files = tab_->view_files();
	if (file_index < 0 || file_index >= files.cached_files_count)
		return;
	
	update();
}

void IconView::UpdateIndices(const QSet<int> &indices)
{
	update();
}

void IconView::UpdateFileIndexRange(const int start, const int end)
{
	update();
}

void IconView::UpdateScrollRange()
{
	ComputeProportions(icon_dim_);
	auto r = std::max(0.0, icon_dim_.total_h - height());
	vs_->setRange(0, r);
	scroll_page_step_ = icon_dim_.rh;
	vs_->setPageStep(scroll_page_step_);
}

void IconView::wheelEvent(QWheelEvent *evt)
{
	auto y = evt->angleDelta().y();
	const Zoom zoom = (y > 0) ? Zoom::In : Zoom::Out;
	if (evt->modifiers() & Qt::ControlModifier)
	{
		app_->prefs().WheelEventFromMainView(zoom);
	} else {
		repaint_without_delay_ = true;
		const auto vert_dir = (y < 0) ? VDirection::Down : VDirection::Up;
		ScrollByWheel(vert_dir, ScrollBy::LineStep);
		repaint_without_delay_ = false;
	}
}

}
