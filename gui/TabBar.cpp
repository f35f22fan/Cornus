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
	static bool first_time = true;
	if (first_time) {
		first_time = false;
		mtl_trace();
	}
//	const ui::DndType dt = ui::GetDndType(evt->mimeData());
//	if (dt == ui::DndType::None)
//		return;
	
	const auto &pos = evt->pos();
	const int tab_index = tabAt(pos);
	if (tab_index != -1) {
		app_->SelectTabAt(tab_index);
	}
}

void TabBar::dropEvent(QDropEvent *evt)
{
	const auto &pos = evt->pos();
	const int tab_index = tabAt(pos);
	if (tab_index == -1)
		return;
	
mtl_trace();
	gui::Tab *tab = app_->tab_at(tab_index);
	tab->DropEvent(evt, ForceDropToCurrentDir::Yes);
}

void TabBar::Init()
{
	setAcceptDrops(true);
}

} // cornus::gui
