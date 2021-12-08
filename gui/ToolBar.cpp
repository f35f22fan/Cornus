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

ToolBar::~ToolBar() {
	for (auto *next: actions_) {
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

void ToolBar::CreateGui()
{
	action_back_ = Add(actions::GoBack, QLatin1String("go-previous"));
	action_fwd_ = Add(actions::GoForward, QLatin1String("go-next"));
	action_up_ = Add(actions::GoUp, QLatin1String("go-up"));
	Add(actions::GoHome, QLatin1String("go-home"));
	
	location_ = new Location(app_);
	addWidget(location_);
	
	view_action_ = new QAction();
	addAction(view_action_);
	connect(view_action_, &QAction::triggered, this, &ToolBar::ViewChanged);
	
	QToolButton* prefs_menu_btn = new QToolButton(this);
	addWidget(prefs_menu_btn);
	prefs_menu_btn->setIcon(QIcon::fromTheme(QLatin1String("preferences-other")));
	prefs_menu_btn->setPopupMode(QToolButton::InstantPopup);
	prefs_menu_ = new QMenu(prefs_menu_btn);
	prefs_menu_btn->setMenu(prefs_menu_);
	
	Add(prefs_menu_, tr("Preferences.."), actions::Preferences,
		QLatin1String("preferences-other"));
	Add(prefs_menu_, tr("Shortcuts Map"), actions::ShortcutsMap,
		QLatin1String("format-justify-left"));
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
	} else if (action == actions::ShortcutsMap) {
		ShowShortcutsMap();
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

void ToolBar::ShowShortcutsMap()
{
	QString s;
	
	s += QLatin1String("Alt+Up => ") + tr("Move one directory up\n");
	s += QLatin1String("Ctrl+H => ") + tr("Toggle show hidden files\n");
	s += QLatin1String("Ctrl+Q => ") + tr("Quit app\n");
	s += QLatin1String("Shift+Delete => ") + tr("Delete selected files\n");
	s += QLatin1String("F2 => ") + tr("Rename selected file\n");
	s += QLatin1String("Ctrl+I => ") + tr("Focus files table view\n");
	s += QLatin1String("Ctrl+L => ") + tr("Focus address bar\n");
	s += QLatin1String("Ctrl+A => ") + tr("Select all files\n");
	s += QLatin1String("Ctrl+E => ") + tr("Toggle exec bit of selected file(s)\n");
	s += QLatin1String("D => ") + tr("Display contents of selected file\n");
	s += QLatin1String("Ctrl+F => ") + tr("Search for file by name, then hit Enter\n"
	"    to search forward or Ctrl+Enter for backwards\n");
	
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

void ToolBar::ViewChanged()
{
	Tab *tab = app_->tab();
	const auto view_was = tab->view_mode();
	ViewMode mode = ViewMode::None;
	
	if (view_was == ViewMode::Details)
	{
		mode = ViewMode::Icons;
		AdjustGui(mode);
	} else if (view_was == ViewMode::Icons || view_was == ViewMode::None) {
		mode = ViewMode::Details;
		AdjustGui(mode);
	}
	
	if (view_was != ViewMode::None) {
		tab->SetViewMode(mode);
	}
}

void ToolBar::AdjustGui(const ViewMode new_mode)
{
	if (new_mode == ViewMode::Details) {
		if (details_view_icon_.isNull()) {
			details_view_icon_ = QIcon::fromTheme(DetailsViewStr);
		}
		view_action_->setIcon(details_view_icon_);
		view_action_->setToolTip(tr("Details View"));
	} else if (new_mode == ViewMode::Icons) {
		if (icons_view_icon_.isNull()) {
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
