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
	void SyncViewModeWithCurrentTab();
	
protected:
	void ViewChanged();
	
private:
	QAction* Add(QMenu *menu, const QString &text, const QString &action_name,
		const QString &icon_name = QString());
	QAction* Add(const QString &action_name, const QString &icon_name = QString());
	void AdjustGui(const ViewMode mode);
	void CreateGui();
	void ProcessAction(const QString &action);
	void ShowAboutThisAppDialog();
	void ShowShortcutsMap();
	
	QVector<QAction*> actions_;
	cornus::App *app_ = nullptr;
	gui::Location *location_ = nullptr;
	QAction *action_fwd_ = nullptr, *action_up_ = nullptr,
		*action_back_ = nullptr, *view_action_ = nullptr;
	QIcon details_view_icon_, icons_view_icon_;
	
	QMenu *history_menu_ = nullptr, *prefs_menu_ = nullptr;
};

}
