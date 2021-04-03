#include "TextField.hpp"

#include <QTimer>

namespace cornus::gui {

TextField::TextField(QWidget *parent)
: QLineEdit(parent)
{}

TextField::~TextField()
{}

void TextField::focusInEvent(QFocusEvent *e)
{
	QLineEdit::focusInEvent(e);
	if (select_all_on_focus_)
		QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

}
