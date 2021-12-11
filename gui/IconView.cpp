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
#include <QScrollArea>

namespace cornus::gui {

IconView::IconView(App *app, Table *table, QScrollArea *sa):
	app_(app), table_(table), scroll_area_(sa)
{
	Q_UNUSED(app_);
	Q_UNUSED(table_);
	Q_UNUSED(zoom_);
}

IconView::~IconView()
{
	
}

QSize IconView::minimumSize() const
{
	return scroll_area_->viewport()->size();
}

QSize IconView::maximumSize() const
{
	const int h = (icon_dim_.size + icon_dim_.in_between) * icon_dim_.row_count;
	const int w = scroll_area_->viewport()->size().width();
	return QSize(w, h);
}

QSize IconView::sizeHint() const
{
	if (icon_dim_.size == -1)
		return QSize(50, 50);
	const int h = (icon_dim_.size + icon_dim_.in_between) * icon_dim_.row_count;
	const int w = scroll_area_->viewport()->size().width();
	return QSize(w, h);
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

void IconView::wheelEvent(QWheelEvent *evt)
{
	if (evt->modifiers() & Qt::ControlModifier)
	{
		auto y = evt->angleDelta().y();
		const Zoom zoom = (y > 0) ? Zoom::In : Zoom::Out;
		app_->prefs().WheelEventFromMainView(zoom);
		evt->ignore();
	} else {
		QWidget::wheelEvent(evt);
	}
}

void PerRow(const float area_w, const float unit_w, const float str_h,
	IconDim &icon_dim)
{
	icon_dim.size = unit_w;
	icon_dim.gap = unit_w / 16;
	const auto gaps = icon_dim.gap * 2;
	icon_dim.max_w = unit_w - gaps;
	icon_dim.max_icon_h = icon_dim.size - (icon_dim.gap * 3) - str_h;
	icon_dim.in_between = gaps * 2;
	
	float full_one = unit_w + icon_dim.in_between;
	icon_dim.per_row = area_w / full_one;
	int rem = int(area_w) % int(full_one);
	if (rem <= 2) {
		mtl_info("Shrinking");
		icon_dim.in_between = 1;
		full_one = unit_w + icon_dim.in_between;
		icon_dim.per_row = area_w / full_one;
	}
	
	if (icon_dim.per_row == 0)
		icon_dim.per_row = 1;
}

void IconView::paintEvent(QPaintEvent *ev)
{
	static const QString sample_str = QLatin1String("abcDEFjhgkml");
	const QFontMetrics fm = fontMetrics();
	QRect br = fm.boundingRect(sample_str);
	const float unit_w = br.width();
	const float unit_h = unit_w;
	const float area_w = width();
	PerRow(area_w, unit_w, br.height(), icon_dim_);
	
	io::Files &files = view_files();
	auto guard = files.guard();
	auto &vec = files.data.vec;
	const int fc = vec.size();
	icon_dim_.row_count = fc / icon_dim_.per_row;
	if (fc % icon_dim_.per_row)
		icon_dim_.row_count++;
	
	if (last_.row_count != icon_dim_.row_count)
	{
		last_.row_count = icon_dim_.row_count;
		int new_h = (icon_dim_.size + icon_dim_.in_between) * icon_dim_.row_count;
		resize(scroll_area_->viewport()->size().width(), new_h);
		return;
	}
	
	QPainter painter(this);
	RestorePainter rp(&painter);
	QStyleOptionViewItem option = table_->view_options();
	
	painter.fillRect(ev->rect(), QColor(0, 255, 0));//option.palette.brush(QPalette::Base));
//	QRect r = ev->rect();
//	mtl_trace("%d,%d,%d,%d", r.x(), r.y(), r.width(), r.height());
	painter.setFont(font());
	double y = unit_h;
	Q_UNUSED(y);
	
//	int index = 0;
//	for (io::File *next: vec)
//	{
//		int x = i * (icon_dim.size + icon_dim.in_between);
//		painter.fillRect(QRect(x,  y - icon_dim.size, icon_dim.size, icon_dim.size),
//			option.palette.brush(QPalette::AlternateBase));
//		io::File *file = vec[i];
		
//		const float text_y = y - icon_dim.gap;
//		painter.drawText(x + icon_dim.gap, text_y, file->name());
		
//		index++;
//	}
}

io::Files& IconView::view_files() {
	return *app_->files(table_->tab()->files_id());
}


}
