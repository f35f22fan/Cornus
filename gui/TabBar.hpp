#pragma once

#include <QTabBar>

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

namespace cornus::gui {

class TabBar: public QTabBar {
public:
	TabBar(App *app);
	virtual ~TabBar();
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *evt) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *evt) override;
	virtual void dropEvent(QDropEvent *evt) override;
	
private:
	NO_ASSIGN_COPY_MOVE(TabBar);
	
	void Init();
	
	App *app_ = nullptr;
};


}
