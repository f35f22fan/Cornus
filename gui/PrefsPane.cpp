#include "PrefsPane.hpp"

#include "../App.hpp"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "TableModel.hpp"

#include <QCheckBox>
#include <QBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLineEdit>

namespace cornus::gui {

PrefsPane::PrefsPane(App *app): QDialog(app), app_(app)
{
	CreateGui();
	setWindowTitle(tr("Preferences"));
	setModal(true);
	setVisible(true);
}

PrefsPane::~PrefsPane()
{
	delete button_box_;
	delete mark_extended_attrs_disabled_;
	delete show_hidden_files_;
	delete show_ms_files_loaded_;
	delete show_free_partition_space_;
	delete show_link_targets_;
	delete remember_window_size_;
}

void PrefsPane::ApplyToWidgets(const Prefs &prefs)
{
	show_hidden_files_->setCheckState(prefs.show_hidden_files() ? Qt::Checked : Qt::Unchecked);
	show_ms_files_loaded_->setCheckState(prefs.show_ms_files_loaded() ? Qt::Checked : Qt::Unchecked);
	show_free_partition_space_->setCheckState(prefs.show_free_partition_space() ? Qt::Checked : Qt::Unchecked);
	show_link_targets_->setCheckState(prefs.show_link_targets() ? Qt::Checked : Qt::Unchecked);
	mark_extended_attrs_disabled_->setCheckState(prefs.mark_extended_attrs_disabled() ? Qt::Checked : Qt::Unchecked);
	remember_window_size_->setCheckState(prefs.remember_window_size() ? Qt::Checked : Qt::Unchecked);
}

void PrefsPane::ButtonClicked(QAbstractButton *btn)
{
	if (btn == button_box_->button(QDialogButtonBox::RestoreDefaults)) {
		ApplyToWidgets(app_->prefs().Defaults());
	} else if (btn == button_box_->button(QDialogButtonBox::Cancel)) {
		close();
	} else if (btn == button_box_->button(QDialogButtonBox::Ok)) {
		SavePrefs();
		close();
	}
}

void PrefsPane::CreateGui()
{
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(vert_layout);
	
	show_hidden_files_ = new QCheckBox(tr("Show hidden files (Ctrl+H)"));
	vert_layout->addWidget(show_hidden_files_);
	
	show_ms_files_loaded_ = new QCheckBox(tr("Show folder listing speed (ms)"));
	vert_layout->addWidget(show_ms_files_loaded_);
	
	show_free_partition_space_ = new QCheckBox(tr("Show free partition space"));
	vert_layout->addWidget(show_free_partition_space_);
	
	show_link_targets_ = new QCheckBox(tr("Show symlink targets"));
	vert_layout->addWidget(show_link_targets_);
	
	mark_extended_attrs_disabled_ = new QCheckBox(tr("Don't mark files with extended file attributes"));
	mark_extended_attrs_disabled_->setToolTip(tr("Shows a gray dot next to the file name in the icon column"));
	vert_layout->addWidget(mark_extended_attrs_disabled_);
	
	remember_window_size_ = new QCheckBox(tr("Remember window size"));
	vert_layout->addWidget(remember_window_size_);
	
	button_box_ = new QDialogButtonBox (QDialogButtonBox::Ok
		| QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Cancel);
	connect(button_box_, &QDialogButtonBox::clicked, this, &PrefsPane::ButtonClicked);
	vert_layout->addWidget(button_box_);
	
	ApplyToWidgets(app_->prefs());
	
	resize(700, 500);
	exec();
}

void PrefsPane::SavePrefs()
{
	Prefs &prefs = app_->prefs();
	const bool show_hidden_files = prefs.show_hidden_files();
	bool show_ms_files_loaded = prefs.show_ms_files_loaded();
	bool show_free_space = prefs.show_free_partition_space();
	
	prefs.show_hidden_files(show_hidden_files_->checkState() == Qt::Checked);
	prefs.show_ms_files_loaded(show_ms_files_loaded_->checkState() == Qt::Checked);
	prefs.show_free_partition_space(show_free_partition_space_->checkState() == Qt::Checked);
	prefs.show_link_targets(show_link_targets_->checkState() == Qt::Checked);
	prefs.mark_extended_attrs_disabled(mark_extended_attrs_disabled_->checkState() == Qt::Checked);
	prefs.remember_window_size(remember_window_size_->checkState() == Qt::Checked);
	prefs.Save();
	
	if (show_ms_files_loaded != prefs.show_ms_files_loaded() ||
		show_hidden_files != prefs.show_hidden_files())
	{
		app_->Reload();
	}
	
	if (show_free_space != prefs.show_free_partition_space())
		app_->table_model()->UpdateHeaderNameColumn();
}

}


