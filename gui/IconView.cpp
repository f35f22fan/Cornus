#include "IconView.hpp"

#include "../App.hpp"
#include "../Hid.hpp"
#include "../io/File.hpp"
#include "../Prefs.hpp"
#include "RestorePainter.hpp"
#include "Tab.hpp"
#include "Table.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImageReader>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace cornus::gui {

IconView::IconView(App *app, Tab *tab, QScrollBar *vs):
	app_(app), tab_(tab), vs_(vs)
{
	Q_UNUSED(app_);
	Q_UNUSED(zoom_);
	Init();
}

IconView::~IconView()
{}

void IconView::ComputeProportions(IconDim &dim) const
{
	const QString sample_str = QLatin1String("m");
	const QFontMetrics fm = fontMetrics();
	const QRect br = fm.boundingRect(sample_str);
	const float area_w = this->width();
	const float w = br.width() * 8;
	
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
	dim.per_row = (int)area_w / (int)dim.cell_and_gap;
	
	if (dim.per_row == 0)
		dim.per_row = 1;
	
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
		if (icon == nullptr)
			return DrawBorder::No;
		
		pixmap = file->cache().icon->pixmap(128, 128);
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

void IconView::DisplayingNewDirectory(const DirId dir_id)
{
	if (last_thumbnail_submission_for_ == dir_id)
		return;
	
	if (last_cancelled_except_ != dir_id)
	{
		last_cancelled_except_ = dir_id;
		app_->RemoveAllThumbTasksExcept(dir_id);
	}
	
	if (is_current_view()) {
		SendLoadingNewThumbnailsBatch();
		UpdateScrollRange();
		vs_->setValue(0);
	}
}

void IconView::FilesChanged(const Repaint r, const int file_index)
{
	if (r == Repaint::IfViewIsCurrent && is_current_view())
	{
		UpdateScrollRange();
		update();
	} else {
		RepaintLater();
	}
}

void IconView::Init()
{
	setFocusPolicy(Qt::WheelFocus);
	/// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	
	connect(vs_, &QScrollBar::valueChanged, [=](int value) {
		if (repaint_without_delay_)
			update();
		else
			RepaintLater();
	});
	
	connect(vs_, &QScrollBar::sliderReleased, [=] {
		update();
	});
}

bool IconView::is_current_view() const
{
	return tab_->view_mode() == ViewMode::Icons;
}

io::File* IconView::GetFileAtNTS(const QPoint &pos, const Clone c, int *ret_index)
{
	const int y = pos.y() + vs_->value();
	int y_off;
	const int row = std::max(0, GetRowAtY(y, &y_off) - 1);
	const int x = pos.x();
	const int wide = icon_dim_.cell_and_gap;
	const int x_index = x / wide;
	
	const int file_index = row * icon_dim_.per_row + x_index;
	auto &files = tab_->view_files();
	if (file_index < 0 || file_index >= files.data.vec.size())
		return nullptr;
	
	if (ret_index)
		*ret_index = file_index;
	
	io::File *f = files.data.vec[file_index];
	
	return (c == Clone::Yes) ? f->Clone() : f;
}

void IconView::keyPressEvent(QKeyEvent *event)
{
	const int key = event->key();
	const auto modifiers = event->modifiers();
	const bool any_modifiers = (modifiers != Qt::NoModifier);
	const bool shift = (modifiers & Qt::ShiftModifier);
	const bool ctrl = (modifiers & Qt::ControlModifier);
	Q_UNUSED(shift);
	Q_UNUSED(ctrl);
	
	if (!any_modifiers)
	{
		if (key == Qt::Key_Down) {
			Scroll(VDirection::Down, ScrollBy::LineStep);
		} else if (key == Qt::Key_Up) {
			Scroll(VDirection::Up, ScrollBy::LineStep);
		} else if (key == Qt::Key_PageDown) {
			Scroll(VDirection::Down, ScrollBy::PageStep);
		} else if (key == Qt::Key_PageUp) {
			Scroll(VDirection::Up, ScrollBy::PageStep);
		}
	}
}

void IconView::keyReleaseEvent(QKeyEvent *evt)
{
	bool shift = evt->modifiers() & Qt::ShiftModifier;
	Q_UNUSED(shift);
}

void IconView::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QVector<int> indices;
	int row;
	io::File *file = GetFileAtNTS(evt->pos(), Clone::Yes, &row);
	if (file) {
		app_->FileDoubleClicked(file, Column::FileName);
		indices.append(row);
		UpdateIndices(indices);
	}
}

void IconView::mouseMoveEvent(QMouseEvent *evt)
{
	
}

void IconView::mousePressEvent(QMouseEvent *evt)
{
	const auto modif = evt->modifiers();
	const bool ctrl = modif & Qt::ControlModifier;
	//const bool shift = modif & Qt::ShiftModifier;
	//const bool right_click = evt->button() == Qt::RightButton;
	const bool left_click = evt->button() == Qt::LeftButton;
	QVector<int> indices;
	
	if (left_click) {
		if (ctrl) {
			app_->hid()->HandleMouseSelectionCtrl(tab_, evt->pos(), &indices);
		}
	}
	
	UpdateIndices(indices);
}

void IconView::mouseReleaseEvent(QMouseEvent *evt)
{
	
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
	const int at_row = (int)scroll_y / (int)cell.rh;
	const int int_off = (int)scroll_y % (int)cell.rh;
	const double y_off = -int_off;
	const double y_end = std::abs(y_off) + height();
	const double width = (this->width() < cell.cell_and_gap) ?
		cell.cell_and_gap : this->width();
	
	io::Files &files = tab_->view_files();
	const int file_count = files.cached_files_count;
	int file_index = at_row * cell.per_row;
	QTextOption text_options;
	text_options.setAlignment(Qt::AlignHCenter);
	text_options.setWrapMode(QTextOption::WrapAnywhere);
	
	for (double y = y_off; y < y_end; y += cell.rh)
	{
		for (double x = 0.0; (x + cell.w_and_gaps) < width; x += cell.cell_and_gap)
		{
			if (file_index >= file_count)
				break;
			
			QString file_name;
			const QRect cell_r(x, y, cell.w_and_gaps, cell.h_and_gaps);
			bool mouse_over = false, file_selected = false;
			DrawBorder draw_border = DrawBorder::No;
			{
				auto guard = files.guard();
				io::File *file = files.data.vec[file_index++];
				file_name = file->name();
				file_selected = file->selected();
				if (file_selected || mouse_over)
				{
					QColor c = option.palette.highlight().color();
					if (mouse_over && !file->selected()) {
						c = app_->hover_bg_color_gray(c);
					}
					
					painter.fillRect(cell_r, c);
				}
				
				draw_border = DrawThumbnail(file, painter, x, y);
			}
			
			const float text_y = y + cell.text_y;
			QRectF text_space(x, text_y, cell.w_and_gaps, cell.text_h);
			//painter.fillRect(text_space, QColor(255, 255, 100));
			painter.drawText(text_space, file_name, text_options);
			
			if (draw_border == DrawBorder::Yes)
			{
				painter.setPen(QColor(100, 100, 100));
				painter.drawRect(cell_r);
			}
		}
		
		if (file_index >= file_count)
			break;
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

void IconView::Scroll(const VDirection d, const gui::ScrollBy sb)
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
	if (icon_dim_.per_row <= 0)
		ComputeProportions(icon_dim_);
	
	int row = file_index / icon_dim_.per_row;
	if (file_index % icon_dim_.per_row)
		row++;
	const int scroll_y = row * icon_dim_.rh;
	vs_->setValue(scroll_y);
}

void IconView::SendLoadingNewThumbnailsBatch()
{
	auto &files = tab_->view_files();
	files.Lock();
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
		auto guard = files.guard();
		for (io::File *file: files.data.vec)
		{
			if (!file->ShouldTryLoadingThumbnail())
				continue;
			
			file->cache().tried_loading_thumbnail = true;
			ThumbLoaderArgs *arg = new ThumbLoaderArgs();
			arg->app = app_;
			arg->ba = file->thumbnail_attrs();
			arg->full_path = file->build_full_path();
			arg->file_id = file->id();
			arg->ext = file->cache().ext.toLocal8Bit();
			arg->tab_id = tab_->id();
			arg->dir_id = dir_id;
			arg->icon_w = max_img_w;
			arg->icon_h = max_img_h;
			//mtl_info("w: %d, h: %d", max_img_w, max_img_h);
			work_stack->append(arg);
		}
	}
	
	app_->SubmitThumbLoaderBatchFromTab(work_stack, tab_->id(), dir_id);
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

void IconView::UpdateIndices(const QVector<int> &indices)
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
		Scroll(vert_dir, ScrollBy::LineStep);
		repaint_without_delay_ = false;
	}
}

}
