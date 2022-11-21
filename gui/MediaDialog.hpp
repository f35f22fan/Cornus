#pragma once

#include "../decl.hxx"
#include "../media.hxx"

#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>

namespace cornus::gui {
class MediaDialog: public QDialog {
public:
	MediaDialog(App *app);
	virtual ~MediaDialog();
	
private:
	NO_ASSIGN_COPY_MOVE(MediaDialog);
	void AddNewItem();
	void ButtonClicked(QAbstractButton *button);
	media::Field GetCurrentCategory() const;
	i64 GetCurrentID() const;
	void Init();
	void Save();
	void SetCurrentByData(QComboBox *cb, ci64 ID,
		const QVector<QString> *names = nullptr);
	void UpdateCurrentCategoryItems(ci64 select_id = -1);
	void UpdateNamesFields();

	App *app_ = nullptr;
	QComboBox *categories_cb_ = nullptr;
	QComboBox *category_items_cb_ = nullptr;
	QLineEdit *native_lang_le_ = nullptr;
	QLineEdit *orig_lang_le_ = nullptr;
	QPushButton *add_as_new_btn_ = nullptr;
	QDialogButtonBox *button_box_ = nullptr;
};
}
