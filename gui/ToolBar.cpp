#include "ToolBar.hpp"

#include "../App.hpp"
#include "actions.hxx"
#include "Location.hpp"
#include "PrefsPane.hpp"

#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QTextEdit>
#include <QToolButton>

namespace cornus::gui {

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
	connect(p, &QAction::triggered, [=] {ProcessAction(action_name);});
	actions_.append(p);
	return p;
}

void
ToolBar::Add(QMenu *menu, const QString &icon_name, const QString &text,
	const QString &action_name)
{
	auto *a = new QAction(QIcon::fromTheme(icon_name), text);
	connect(a, &QAction::triggered, [=] {ProcessAction(action_name);});
	menu->addAction(a);
}

void ToolBar::CreateGui()
{
	Add(QLatin1String("go-previous"), actions::GoBack);
	Add(QLatin1String("go-up"), actions::GoUp);
	Add(QLatin1String("go-home"), actions::GoHome);
	
	location_ = new Location(app_);
	addWidget(location_);
	
	QToolButton* prefs_menu_btn = new QToolButton(this);
	addWidget(prefs_menu_btn);
	prefs_menu_btn->setIcon(QIcon::fromTheme(QLatin1String("preferences-other")));
	prefs_menu_btn->setPopupMode(QToolButton::InstantPopup);
	QMenu *prefs_menu = new QMenu(prefs_menu_btn);
	prefs_menu_btn->setMenu(prefs_menu);
	
	Add(prefs_menu, QLatin1String("preferences-other"), tr("Preferences.."),
		actions::Preferences);
	Add(prefs_menu, QLatin1String("help-about"), tr("About"), actions::AboutThisApp);
	
}

void ToolBar::ProcessAction(const QString &action)
{
	if (action == actions::GoUp) {
		app_->GoUp();
	} else if (action == actions::GoHome) {
		app_->GoHome();
	} else if (action == actions::GoBack) {
		app_->GoBack();
	} else if (action == actions::AboutThisApp) {
		ShowAboutThisAppDialog();
	} else if (action == actions::Preferences) {
		gui::PrefsPane pp(app_);
	}
}

void ToolBar::ShowAboutThisAppDialog()
{
	QDialog dialog(this);
	dialog.setWindowTitle(tr("About this app"));
	dialog.setModal(true);
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	dialog.setLayout(vert_layout);
	
	QTextEdit *te = new QTextEdit();
	te->setReadOnly(true);
	vert_layout->addWidget(te);
	QString str = "<center><b>Cornus Mas</b><br/>A fast file browser"
	" for Linux written in C++17 & Qt5<hr/>"
	"Author: f35f22fan@gmail.com<br/>"
	"GitHub: https://github.com/f35f22fan/Cornus<br/><br/><br/>"
	"<img src=\":resources/cornus.webp\" width=256 height=256></img>"
	"<br/><br/><i><small>Cornus Mas is also a wild and "
	"relatively rare tree & fruit that the author of this app happens "
	"to cultivate that tastes as a mixture of cranberry and sour cherry."
	"</small></i></center>";
	te->setText(str);
	
	{
		QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);
		connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		//connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		vert_layout->addWidget(button_box);
	}
	
	dialog.resize(600, 450);
	dialog.exec();
//	bool ok = dialog.exec();
//	if (!ok)
//		return;
}

}
