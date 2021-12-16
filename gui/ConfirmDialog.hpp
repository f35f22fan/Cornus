#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QHash>
#include <QLabel>

QT_BEGIN_NAMESPACE
class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
QT_END_NAMESPACE

namespace cornus::gui {

class ConfirmDialog: public QDialog {
public:
	ConfirmDialog(App *app, const QHash<IOAction, QString> &h,
		const IOAction default_action, QString &password);
	virtual ~ConfirmDialog();
	
	void SetMessage(const QString &s);
	void SetComboLabel(const QString &s);
	QVariant combo_value() const;
	QString input_text() const;
	
private:
	NO_ASSIGN_COPY_MOVE(ConfirmDialog);
	
	void ButtonClicked(QAbstractButton *btn);
	void CreateGui(const QHash<IOAction, QString> &h, const IOAction default_action);
	
	App *app_ = nullptr;
	QComboBox *cb_ = nullptr;
	QLabel *msg_label_ = nullptr;
	QLabel *combo_label_ = nullptr;
	QDialogButtonBox *button_box_ = nullptr;
	QString &password_;
	QLineEdit *lineedit_ = nullptr;
};


}
