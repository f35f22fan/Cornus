#include "ToolBar.hpp"

#include "../App.hpp"
#include "Location.hpp"

namespace cornus::gui {

const QString kActionGoHome = QLatin1String("Go Home");
const QString kActionGoBack = QLatin1String("Go Back");
const QString kActionGoUp = QLatin1String("Go Up");
const QString kPreferences = QLatin1String("Preferences");

ToolBar::ToolBar(cornus::App *app): app_(app) {
	CreateGui();
}

ToolBar::~ToolBar() {
	for (auto *next: actions_) {
		delete next;
	}
}

QAction*
ToolBar::Add(const QString &icon_name, const QString &action_name)
{
	auto *p = addAction(QIcon::fromTheme(icon_name), QString());
	connect(p, &QAction::triggered,
			[=] {ProcessAction(action_name);});
	actions_.append(p);
	return p;
}

void ToolBar::CreateGui() {
	Add(QLatin1String("go-previous"), kActionGoBack);
	Add(QLatin1String("go-up"), kActionGoUp);
	Add(QLatin1String("go-home"), kActionGoHome);
	
	location_ = new Location(app_);
	addWidget(location_);
	
	auto *p = Add(QLatin1String("preferences-other"), kPreferences);
	p->setToolTip("Preferences");
}

void ToolBar::ProcessAction(const QString &action)
{
	if (action == kActionGoUp) {
		app_->GoUp();
	} else if (action == kActionGoHome) {
		app_->GoHome();
	} else if (action == kActionGoBack) {
		app_->GoBack();
	}
}

}
