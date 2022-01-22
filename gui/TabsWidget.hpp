#pragma once

#include <QTabWidget>

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

#include "TabBar.hpp"

namespace cornus::gui {

class TabsWidget: public QTabWidget {
public:
	TabsWidget();
	virtual ~TabsWidget();
	
	void Init();
	
	void SetTabBar(QTabBar *p) { setTabBar(p); Init();}
private:
	NO_ASSIGN_COPY_MOVE(TabsWidget);
	
	
};


}
