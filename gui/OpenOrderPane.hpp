#pragma once

#include <QDialog>
#include <QLocale>

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

QT_BEGIN_NAMESPACE
class QAbstractButton;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

namespace cornus::gui {

enum class Direction: i8 {
	Up,
	Down
};

class OpenOrderPane: public QDialog {
public:
	OpenOrderPane(App *app, gui::Tab *tab);
	virtual ~OpenOrderPane();
	
	OpenOrderModel* model() const { return model_; }
	OpenOrderTable* table() const { return table_; }
	
	const QString& mime() const { return mime_; }
	
protected:
	void TableSelectionChanged();

private:
	NO_ASSIGN_COPY_MOVE(OpenOrderPane);
	
	void AddSelectedCustomItem();
	void AskAddCustomExecutable();
	void ButtonClicked(QAbstractButton *button);
	void ClearData();
	QWidget *CreateAddingCustomItem();
	QDialogButtonBox* CreateButtonsPane();
	void CreateGui();
	int GetSelectedRowIndex();
	void MoveItem(const Direction d);
	void QueryData();
	void RemoveSelectedItem();
	void RestoreDefaults();
	void Save();
	void UpdateRemovedList();
	bool WasOrderModified() const;
	
	App *app_ = nullptr;
	QDialogButtonBox *button_box_ = nullptr;
	OpenOrderTable *table_ = nullptr;
	OpenOrderModel *model_ = nullptr;
	QVector<DesktopFile*> open_with_original_vec_;
	QVector<DesktopFile*> hide_vec_;
	QVector<DesktopFile*> all_desktop_files_;
	DesktopFile *custom_binary_ = nullptr;
	QString mime_;
	QPushButton *up_btn_ = nullptr, *down_btn_ = nullptr, *remove_btn_ = nullptr;
	QComboBox *add_custom_cb_ = nullptr;
	QLineEdit *removed_tf_ = nullptr;
	gui::Tab *tab_ = nullptr;
	QLocale locale_;
};
}
