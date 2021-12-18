#include "ConfirmDialog.hpp"

#include "../App.hpp"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

namespace cornus::gui {

ConfirmDialog::ConfirmDialog(App *app, const QHash<IOAction, QString> &h,
	const IOAction default_action)
: QDialog(app), app_(app)
{
	Q_UNUSED(app_);
	CreateGui(h, default_action);
	adjustSize();
}

ConfirmDialog::~ConfirmDialog()
{}

void ConfirmDialog::ButtonClicked(QAbstractButton *btn)
{
	if (btn == button_box_->button(QDialogButtonBox::Ok)) {
	} else {
	}
}

QVariant ConfirmDialog::combo_value() const
{
	return (cb_ == nullptr) ? QVariant(int(IOAction::None)) : cb_->currentData();
}

void ConfirmDialog::CreateGui(const QHash<IOAction, QString> &h,
	const IOAction default_action)
{
	setModal(true);
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(vert_layout);
	msg_label_ = new QLabel();
	vert_layout->addWidget(msg_label_, 0, Qt::AlignHCenter);
	QFormLayout *form = new QFormLayout();
	vert_layout->addLayout(form);
	
	if (!h.isEmpty())
	{
		combo_label_ = new QLabel();
		cb_ = new QComboBox();
		combo_label_->setBuddy(cb_);
		
		QHashIterator it(h);
		while (it.hasNext())
		{
			it.next();
			auto key = static_cast<IOActionType>(it.key());
			cb_->addItem(it.value(), key);
		}
		
		if (default_action != IOAction::None)
		{
			const auto as_num = static_cast<IOActionType>(default_action);
			int index = cb_->findData(as_num);
			if (index != -1)
				cb_->setCurrentIndex(index);
		}
		
		form->addRow(combo_label_, cb_);
	}
	
	button_box_ = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	vert_layout->addWidget(button_box_, 0, Qt::AlignHCenter);
	QPushButton *ok_btn = button_box_->button(QDialogButtonBox::Ok);
	connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
	QPushButton *cancel_btn = button_box_->button(QDialogButtonBox::Cancel);
	connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
	connect(button_box_, &QDialogButtonBox::clicked, this, &ConfirmDialog::ButtonClicked);
}

void ConfirmDialog::SetComboLabel(const QString &s)
{
	if (combo_label_ != nullptr)
		combo_label_->setText(s);
}

void ConfirmDialog::SetMessage(const QString &s)
{
	msg_label_->setText(s);
}

}
