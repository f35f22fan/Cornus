#pragma once

#include <QToolBar>

#include "../err.hpp"
#include "decl.hxx"
#include "../decl.hxx"

namespace cornus::gui {

class ToolBar: public QToolBar {
public:
	ToolBar(cornus::App *app);
	virtual ~ToolBar();
	
	gui::Location* location() const { return location_; }
	
private:
	QAction *Add(const QString &icon_name, const QString &action_name);
	void CreateGui();
	void ProcessAction(const QString &action);
	
	QVector<QAction*> actions_;
	cornus::App *app_ = nullptr;
	gui::Location *location_ = nullptr;
};

}
