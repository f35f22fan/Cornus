#include "TabsWidget.hpp"

namespace cornus::gui {

TabsWidget::TabsWidget()
{}

TabsWidget::~TabsWidget() {}

void TabsWidget::Init()
{
	setTabsClosable(true);
	setTabBarAutoHide(true);
}

}
