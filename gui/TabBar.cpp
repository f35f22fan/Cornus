#include "TabBar.hpp"

#include "../App.hpp"
#include "Tab.hpp"
#include "../ui.hh"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>

namespace cornus::gui {

TabBar::TabBar(App *app): app_(app)
{
	Init();
	Q_UNUSED(app_);
}

TabBar::~TabBar()
{}

void TabBar::dragEnterEvent(QDragEnterEvent *evt)
{
	const ui::DndType dt = ui::GetDndType(evt->mimeData());
	if (dt != ui::DndType::None)
		evt->acceptProposedAction();
}

void TabBar::dragLeaveEvent(QDragLeaveEvent *evt)
{}

void TabBar::dragMoveEvent(QDragMoveEvent *evt)
{
	cauto pos = evt->position().toPoint();
	cint tab_index = tabAt(pos);
	if (tab_index != -1) {
		app_->SelectTabAt(tab_index, FocusView::Yes);
	}
}

void TabBar::dropEvent(QDropEvent *evt)
{
	cauto pos = evt->position().toPoint();
	const int tab_index = tabAt(pos);
	if (tab_index == -1)
		return;
	
	gui::Tab *tab = app_->tab_at(tab_index);
	tab->DropEvent(evt, ForceDropToCurrentDir::Yes);
}

void TabBar::Init()
{
	setAcceptDrops(true);
}

} // cornus::gui
