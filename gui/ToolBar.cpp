#include "ToolBar.hpp"

#include "../App.hpp"
#include "actions.hxx"
#include "../History.hpp"
#include "Location.hpp"
#include "MediaDialog.hpp"
#include "PrefsPane.hpp"
#include "Tab.hpp"

#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QTextEdit>
#include <QToolButton>

namespace cornus::gui {

const QString NewMediaEntryAction = QLatin1String("NewMediaEntry");
const QString IconsViewStr = QLatin1String("view-list-icons");
const QString DetailsViewStr = QLatin1String("view-list-details");

ToolBar::ToolBar(cornus::App *app): app_(app) {
	CreateGui();
}

ToolBar::~ToolBar()
{
	for (auto *next: actions_)
	{
		delete next;
	}
	
	delete prefs_menu_;
	delete history_menu_;
}

QAction*
ToolBar::Add(const QString &action_name, const QString &icon_name)
{
	auto *p = addAction(QIcon::fromTheme(icon_name), QString());
	connect(p, &QAction::triggered, [=] {ProcessAction(action_name);});
	actions_.append(p);
	return p;
}

QAction*
ToolBar::Add(QMenu *menu, const QString &text,
	const QString &action_name, const QString &icon_name)
{
	QAction *a = nullptr;
	if (icon_name.isEmpty())
		a = menu->addAction(text);
	else
		a = menu->addAction(QIcon::fromTheme(icon_name), text);
	connect(a, &QAction::triggered, [=] {ProcessAction(action_name);});
	return a;
	
}

void ToolBar::ChangeView()
{
	Tab *tab = app_->tab();
	cauto from_mode = tab->view_mode();
	ViewMode to_mode = ViewMode::None;
	
	if (from_mode == ViewMode::Details)
	{
		to_mode = ViewMode::Icons;
		AdjustGui(to_mode);
	} else if (from_mode == ViewMode::Icons || from_mode == ViewMode::None) {
		to_mode = ViewMode::Details;
		AdjustGui(to_mode);
	}
	
	if (from_mode != ViewMode::None) {
		tab->SetViewMode(to_mode);
	}
}

void ToolBar::CreateGui()
{
	action_back_ = Add(actions::GoBack, QLatin1String("go-previous"));
	action_fwd_ = Add(actions::GoForward, QLatin1String("go-next"));
	action_up_ = Add(actions::GoUp, QLatin1String("go-up"));
	Add(actions::GoHome, QLatin1String("go-home"));
	
	location_ = new Location(app_);
	addWidget(location_);
	
	view_action_ = new QAction();
	actions_.append(view_action_);
	addAction(view_action_);
	connect(view_action_, &QAction::triggered, this, &ToolBar::ChangeView);
	
	QToolButton* prefs_menu_btn = new QToolButton(this);
	addWidget(prefs_menu_btn);
	prefs_menu_btn->setIcon(QIcon::fromTheme(QLatin1String("preferences-other")));
	prefs_menu_btn->setPopupMode(QToolButton::InstantPopup);
	prefs_menu_ = new QMenu(prefs_menu_btn);
	prefs_menu_btn->setMenu(prefs_menu_);
	
	Add(prefs_menu_, tr("Preferences.."), actions::Preferences,
		QLatin1String("preferences-other"));
	Add(prefs_menu_, tr("&Shortcuts.."), actions::ShowShortcuts,
		QLatin1String("go-next"));
	Add(prefs_menu_, tr("Media Database"), NewMediaEntryAction,
		QLatin1String("contact-new"));
	Add(prefs_menu_, tr("About"), actions::AboutThisApp,
		QLatin1String("help-about"));
}

void ToolBar::ProcessAction(const QString &action)
{
	gui::Tab *tab = app_->tab();
	if (action == actions::GoUp) {
		tab->GoUp();
	} else if (action == actions::GoHome) {
		tab->GoHome();
	} else if (action == actions::GoBack) {
		tab->GoBack();
	} else if (action == actions::GoForward) {
		tab->GoForward();
	} else if (action == actions::AboutThisApp) {
		ShowAboutThisAppDialog();
	} else if (action == actions::Preferences) {
		gui::PrefsPane pp(app_);
	} else if (action == actions::ShowShortcuts) {
		ShowShortcuts();
	} else if (action == NewMediaEntryAction) {
		gui::MediaDialog d(app_);
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
	"Author: f35f22fan@gmail.com (Ursache Vladimir)<br/>"
	"GitHub: https://github.com/f35f22fan/Cornus<br/><br/><br/>"
	"<img src=\"";
	str += AppIconPath;
	str += "\" width=128 height=128></img>"
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

void ToolBar::ShowShortcuts()
{
	QString s(QLatin1String("<html>"));
	
	s += QLatin1String("<b>Ctrl + T</b> => ")  + tr("Open a new tab");
	s += QLatin1String("<br/><b>Ctrl + W</b> => ") + tr("Close current tab");
	s += QLatin1String("<br/><b>1 to 9</b> => ") + tr("Select and focus tab 1 to 9");
	s += QLatin1String("<br/><b>G</b> => ") + tr("Toggle <big>magnified</big> mode");
	s += QLatin1String("<br/><b>Ctrl + R</b> => ") + tr("Reload tab view (list files anew)");
	s += QLatin1String("<br/><b>Alt + Up</b> => ") + tr("Go one directory up");
	s += QLatin1String("<br/><b>Ctrl + H</b> => ") + tr("Toggle showing of hidden files");
	s += QLatin1String("<br/><b>Ctrl + Q</b> => ") + tr("Quit app");
	s += QLatin1String("<br/><b>Shift + Delete</b> => ") + tr("Delete selected files permanently, no confirmation asked.");
	s += QLatin1String("<br/><b>F2</b> => ") + tr("Rename selected file");
	s += QLatin1String("<br/><b>Ctrl + I</b> => ") + tr("Focus table");
	s += QLatin1String("<br/><b>Ctrl + L</b> => ") + tr("Focus address bar");
	s += QLatin1String("<br/><b>Ctrl + A</b> => ") + tr("Select all files");
	s += QLatin1String("<br/><b>E</b> => ") + tr("Toggle exec bit of selected file(s)");
	s += QLatin1String("<br/><b>D</b> => ") + tr("Display contents of selected file");
	s += QLatin1String("<br/><b>V</b> => ") + tr("Switch view");
	s += QLatin1String("<br/><b>Ctrl + F</b> => ") + tr("Search for file by name (then hit Enter to search forward or Ctrl+Enter for backwards)");
	s += QLatin1String("<br/><b>Ctrl + M</b> => ") + tr("Search by (movie) file's metadata (see bottom of page)");
	s += QLatin1String("<br/><b>M</b> => ") + tr("Show media dialog for selected file");
	s += QLatin1String("<br/><b>Ctrl + Shift + U</b> => ") + tr("Remove thumbnail from selected files' extended attributes");
	s += QLatin1String("<br/><b>F1</b> => ") + tr("Mark last watched file (with a red dot in Details View)");
	
	s += QLatin1String("</html>");
	app_->TellUser(s, tr("App Shortcuts"));
}

void ToolBar::UpdateIcons(History *p)
{
	int index, size;
	p->index_size(index, size);
	
	action_back_->setEnabled(index > 0);
	action_fwd_->setEnabled(index < (size - 1));
	action_up_->setEnabled(app_->tab()->current_dir() != QLatin1String("/"));
}

void ToolBar::AdjustGui(const ViewMode new_mode)
{
	if (new_mode == ViewMode::Details)
	{
		if (details_view_icon_.isNull())
		{
			details_view_icon_ = QIcon::fromTheme(DetailsViewStr);
		}
		view_action_->setIcon(details_view_icon_);
		view_action_->setToolTip(tr("Details View"));
	} else if (new_mode == ViewMode::Icons) {
		if (icons_view_icon_.isNull())
		{
			icons_view_icon_ = QIcon::fromTheme(IconsViewStr);
		}
		view_action_->setIcon(icons_view_icon_);
		view_action_->setToolTip(tr("Icons View"));
	}
}

void ToolBar::SyncViewModeWithCurrentTab()
{
	AdjustGui(app_->tab()->view_mode());
}

}
