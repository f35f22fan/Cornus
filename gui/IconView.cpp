#include "IconView.hpp"

#include "../App.hpp"
#include "../io/File.hpp"
#include "../Prefs.hpp"
#include "RestorePainter.hpp"
#include "Tab.hpp"
#include "Table.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>

namespace cornus::gui {

IconView::IconView(App *app, Tab *tab, QScrollBar *vs):
	app_(app), tab_(tab), vs_(vs)
{
	Q_UNUSED(app_);
	Q_UNUSED(zoom_);
}

IconView::~IconView()
{}

void IconView::ComputeProportions(IconDim &dim) const
{
	const QString sample_str = QLatin1String("m");
	const QFontMetrics fm = fontMetrics();
	QRect br = fm.boundingRect(sample_str);
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

DrawBorder IconView::DrawPixmap(const QPixmap &pixmap,
	QPainter &painter, double x, double y, const double text_h)
{
	const auto &cell = icon_dim_;
	const int max_img_h = cell.h_and_gaps - text_h;
	const int max_img_w = cell.w_and_gaps;
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
		UpdateRange();
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
	static bool first_time = true;
	if (first_time || pending_update_)
	{
		first_time = false;
		pending_update_ = false;
		UpdateRange();
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
			const int text_h = cell.str_h * cell.text_rows;
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
			
			DrawBorder draw_border = DrawBorder::No;
			QIcon *icon = app_->GetFileIcon(file);
			if (icon != nullptr)
			{
				QPixmap pixmap = file->cache().icon->pixmap(256, 256);
				draw_border = DrawPixmap(pixmap, painter, x, y, text_h);
			}
			
			const float text_y = y + cell.text_y;
			QRectF text_space(x, text_y, cell.w_and_gaps, text_h);
			painter.fillRect(text_space, QColor(255, 255, 100));
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

void IconView::resizeEvent(QResizeEvent *ev)
{
	QWidget::resizeEvent(ev);
	UpdateRange();
}

QSize IconView::size() const
{
	ComputeProportions(icon_dim_);
	int w = parentWidget()->size().width();
	return QSize(w, icon_dim_.total_h);
}

void IconView::UpdateRange()
{
	ComputeProportions(icon_dim_);
	auto r = std::max(0.0, icon_dim_.total_h - height());
	vs_->setRange(0, r);
	vs_->setPageStep(height() / 2);
}

void IconView::wheelEvent(QWheelEvent *evt)
{
	auto y = evt->angleDelta().y();
	const Zoom zoom = (y > 0) ? Zoom::In : Zoom::Out;
	if (evt->modifiers() & Qt::ControlModifier)
	{
		app_->prefs().WheelEventFromMainView(zoom);
	} else {
		const auto step = icon_dim_.str_h;
		const auto val = vs_->value();
		if (y < 0) {
			vs_->setValue(val + step);
		} else {
			vs_->setValue(std::max(0, val - step));
		}
		update();
	}
}

}
