#include "IconView.hpp"

#include "../App.hpp"
#include "../io/File.hpp"
#include "../Prefs.hpp"
#include "Tab.hpp"
#include "Table.hpp"

#include <QPaintEvent>
#include <QPainter>

namespace cornus::gui {

IconView::IconView(App *app, Table *table):
	app_(app), table_(table)
{
	Q_UNUSED(app_);
	Q_UNUSED(table_);
	Q_UNUSED(zoom_);
}

IconView::~IconView()
{
	
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

void IconView::paintEvent(QPaintEvent *ev)
{
	QWidget::paintEvent(ev);
	QRect r = ev->rect();
	mtl_trace("%d,%d,%d,%d", r.x(), r.y(), r.width(), r.height());
	
	const QFontMetrics fm = fontMetrics();
	QRect br = fm.boundingRect("abcDEFjhgkml");
	const float unit_w = br.width();
	const float unit_h = br.height();
	const float area_w = width();
	int per_row = area_w / unit_w;
	
	io::Files &files = view_files();
	auto guard = files.guard();
	auto &vec = files.data.vec;
	QPainter p(this);
	p.setFont(font());
	double y = unit_h;
	
	for (int i = 0; i < per_row; i++)
	{
		if (i >= vec.size() - 1)
			break;
		int x = i * unit_w;
		io::File *file = vec[i];
		p.drawText(x, y, file->name());
	}
}

io::Files& IconView::view_files() {
	return *app_->files(table_->tab()->files_id());
}


}
