#include "SearchLineEdit.hpp"

#include <QFontMetrics>
#include <QPainter>

namespace cornus::gui {

SearchLineEdit::SearchLineEdit(QWidget *parent): QLineEdit(parent)
{}

SearchLineEdit::~SearchLineEdit() {}

void SearchLineEdit::paintEvent(QPaintEvent *evt)
{
	QLineEdit::paintEvent(evt);
	
	if (found_ == -1)
		return;
	
	auto fm = fontMetrics();
	QString s = '(' + QString::number(found_) + QLatin1String(")   ");
	int w = fm.boundingRect(s).width();
	QPainter painter(this);
	QPen pen(QColor(0, 0, 255));
	///pen.setWidthF(2.0);
	painter.setPen(pen);
	QRectF r(width() - w, 0, w, height());
	QTextOption option;
	option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	painter.drawText(r, s, option);
}

}
