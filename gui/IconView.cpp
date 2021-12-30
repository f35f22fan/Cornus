#include "IconView.hpp"

#include "../App.hpp"
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

DrawBorder IconView::DrawThumbnail(io::File *file, QPainter &painter,
	double x, double y)
{
	const auto &cell = icon_dim_;
	const int max_img_h = cell.h_and_gaps - cell.text_h;
	const int max_img_w = cell.w_and_gaps;
	QPixmap pixmap;
	static const auto formats = QImageReader::supportedImageFormats();
	if (!vs_->isSliderDown() && file->ShouldTryLoadingThumbnail())
	{
		file->cache().tried_loading_thumbnail = true;
		auto ext = file->cache().ext.toLocal8Bit();
		if (!ext.isEmpty() && formats.contains(ext))
		{
			ThumbLoaderArgs *arg = new ThumbLoaderArgs();
			arg->app = app_;
			arg->full_path = file->build_full_path();
			arg->file_id = file->id();
			arg->ext = ext;
			arg->tab_id = tab_->id();
			auto &files = tab_->view_files();
			// files is already locked by the calling method
			arg->dir_id = files.data.dir_id;
			arg->icon_w = max_img_w;
			arg->icon_h = max_img_h;
			//mtl_info("w: %d, h: %d", max_img_w, max_img_h);
			app_->SubmitThumbLoaderWork(arg);
		}
	} else if (file->thumbnail() == nullptr
		&& file->has_thumbnail_attr()) // && !file->IsThumbnailMarkedFailed())
	{
		QImage img = file->CreateThumbnailFromExtAttr();
		if (!img.isNull())
		{
			//mtl_printq(file->name());
			Thumbnail *p = new Thumbnail();
			p->img = img;
			p->w = img.width();
			p->h = img.height();
			p->file_id = file->id_num();
			p->time_generated = time(NULL);
			p->tab_id = tab_->id();
			auto &files = tab_->view_files();
			p->dir_id = files.data.dir_id;
			file->thumbnail(p);
		}
	}
	
	if (file->thumbnail() != nullptr) {
		pixmap = QPixmap::fromImage(file->thumbnail()->img);
	} else {
		QIcon *icon = app_->GetFileIcon(file);
		if (icon == nullptr)
			return DrawBorder::No;
		
		pixmap = file->cache().icon->pixmap(256, 256);
	}
	const double pw = pixmap.width();
	const double ph = pixmap.height();
	double used_w, used_h;
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
	double img_x = x + (max_img_w - used_w) / 2;
	painter.drawPixmap(img_x, y, used_w, used_h, pixmap);
	
	auto area_img = used_w * used_h;
	auto area_avail = max_img_w * max_img_h;
	const auto area_img_ratio = area_avail / area_img;
//	mtl_info("ratio %.1f, used %.1f/%.1f, avail: %d/%d", area_img_ratio,
//		used_w, used_h, max_img_w, max_img_h);
	return (area_img_ratio > 1.45) ? DrawBorder::Yes : DrawBorder::No;
}

void IconView::FilesChanged(const Repaint r, const int file_index)
{
	const bool is_current = (tab_->view_mode() == ViewMode::Icons);
	if (r == Repaint::IfViewIsCurrent && is_current)
	{
		UpdateScrollRange();
		if (file_index != -1) {
			static bool first_time = true;
			if (first_time) {
				first_time = false;
				mtl_tbd();
			}
			update();
		} else {
			update();
		}
	} else {
		pending_update_ = true;
	}
}

void IconView::Init()
{
	setFocusPolicy(Qt::WheelFocus);
	/// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	
	connect(vs_, &QScrollBar::valueChanged, [=](int value) {
		if (!force_repaint_)
		{
			RepaintLater();
		}
	});
	
	connect(vs_, &QScrollBar::sliderReleased, [=] {
		update();
	});
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
	
}

void IconView::mouseMoveEvent(QMouseEvent *evt)
{
	
}

void IconView::mousePressEvent(QMouseEvent *evt)
{
	
}

void IconView::mouseReleaseEvent(QMouseEvent *evt)
{
	
}

void IconView::paintEvent(QPaintEvent *ev)
{
	last_repaint_.Continue(Reset::Yes);
	
	static bool first_time = true;
	if (first_time || pending_update_)
	{
		first_time = false;
		pending_update_ = false;
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
	auto guard = files.guard();
	auto &vec = files.data.vec;
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
			io::File *file = vec[file_index];
			file_index++;
			const QRect cell_r(x, y, cell.w_and_gaps, cell.h_and_gaps);
			bool mouse_over = false;
			if (file->selected() || mouse_over)
			{
				QColor c = option.palette.highlight().color();
				if (mouse_over && !file->selected()) {
					c = app_->hover_bg_color_gray(c);
				}
				
				painter.fillRect(cell_r, c);
			}
			
			DrawBorder draw_border = DrawThumbnail(file, painter, x, y);
			
			const float text_y = y + cell.text_y;
			QRectF text_space(x, text_y, cell.w_and_gaps, cell.text_h);
			//painter.fillRect(text_space, QColor(255, 255, 100));
			painter.drawText(text_space, file->name(), text_options);
			
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

void IconView::RepaintLater()
{
	if (!delayed_repaint_pending_)
	{
		delayed_repaint_pending_ = true;
		QTimer::singleShot(delay_repaint_ms_, this, &IconView::DelayedRepaint);
	} else {
//		static int count = 1;
//		mtl_info("Skipped frame %d", count++);
	}
}

void IconView::resizeEvent(QResizeEvent *ev)
{
	UpdateScrollRange();
	RepaintLater();
}

void IconView::Scroll(const VDirection d, const gui::ScrollBy sb)
{
	const auto step = (sb == ScrollBy::LineStep)
		? icon_dim_.str_h : scroll_page_step_;
	const auto val = vs_->value();
	
	if (d == VDirection::Down)
	{
		vs_->setValue(val + step);
	} else {
		vs_->setValue(std::max(0, val - step));
	}
}

QSize IconView::size() const
{
	ComputeProportions(icon_dim_);
	int w = parentWidget()->size().width();
	return QSize(w, icon_dim_.total_h);
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
		force_repaint_ = true;
		const auto vert_dir = (y < 0) ? VDirection::Down : VDirection::Up;
		Scroll(vert_dir, ScrollBy::LineStep);
		update();
		force_repaint_ = false;
	}
}

}
