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
	
	if (count_ == -1)
		return;
	
	auto fm = fontMetrics();
	const QString s = '(' + QString::number(at_) + '/' +
		QString::number(count_) + ')';
	int w = fm.boundingRect(s).width() + 5;
	QPainter painter(this);
	QPen pen(QColor(0, 0, 255));
	painter.setPen(pen);
	QRectF r(width() - w, 0, w, height());
	QTextOption option;
	option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	painter.drawText(r, s, option);
}

}
