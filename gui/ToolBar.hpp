#pragma once

#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include "../err.hpp"
#include "decl.hxx"
#include "../decl.hxx"

namespace cornus::gui {

class ToolBar: public QToolBar {
public:
	ToolBar(cornus::App *app);
	virtual ~ToolBar();
	
	gui::Location* location() const { return location_; }
	void UpdateIcons(History *p);
	
private:
	QAction* Add(QMenu *menu, const QString &icon_name, const QString &text, const QString &action_name);
	QAction* Add(const QString &icon_name, const QString &action_name);
	void CreateGui();
	void ProcessAction(const QString &action);
	void ShowAboutThisAppDialog();
	void ShowShortcutsMap();
	
	QVector<QAction*> actions_;
	cornus::App *app_ = nullptr;
	gui::Location *location_ = nullptr;
	QAction *action_fwd_ = nullptr, *action_up_ = nullptr, *action_back_ = nullptr;
	///QToolButton *back_btn_ = nullptr;
	
	QMenu *history_menu_ = nullptr, *prefs_menu_ = nullptr;
};

}
