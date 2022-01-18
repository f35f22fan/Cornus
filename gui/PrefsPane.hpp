#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

namespace cornus::gui {

class PrefsPane: public QDialog {
public:
	PrefsPane(App *app);
	virtual ~PrefsPane();

private:
	NO_ASSIGN_COPY_MOVE(PrefsPane);
	
	void ApplyToWidgets(const Prefs &prefs);
	void ButtonClicked(QAbstractButton *button);
	void CreateGui();
	void SavePrefs();
	
	App *app_ = nullptr;
	
	QDialogButtonBox *button_box_ = nullptr;
	
	QCheckBox *mark_extended_attrs_disabled_ = nullptr;
	QCheckBox *show_dir_file_count_ = nullptr;
	QCheckBox *show_hidden_files_ = nullptr;
	QCheckBox *show_ms_files_loaded_ = nullptr;
	QCheckBox *show_free_partition_space_ = nullptr;
	QCheckBox *show_link_targets_ = nullptr;
	QCheckBox *remember_window_size_ = nullptr;
};
}
