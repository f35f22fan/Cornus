#include "MediaDialog.hpp"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QFontMetrics>

#include "../App.hpp"
#include "../Media.hpp"
#include "TextField.hpp"

namespace cornus::gui {

MediaDialog::MediaDialog(App *app): QDialog(app), app_(app)
{
	mtl_check_void(Init());
	resize(800, 600);
	setModal(true);
	setWindowTitle(tr("Media database"));
	exec();
}

MediaDialog::~MediaDialog()
{
	delete categories_cb_;
	delete category_items_cb_;
	delete native_lang_le_;
	delete orig_lang_le_;
	delete button_box_;
}

void MediaDialog::AddNewItem()
{
	QString v1 = orig_lang_le_->text().trimmed();
	QString v2 = native_lang_le_->text().trimmed();
	
	if (v1.isEmpty() && v2.isEmpty()) {
		app_->TellUser(tr("No values supplied for native & original fields"));
		return;
	}
	
	QVector<QString> names = {v1, v2};
	Media *media = app_->media();
	
	media::Field f = GetCurrentCategory();
	i64 new_id = -1;
	i64 existing_id = -1;
	{
		auto g = media->guard();
		new_id = media->SetNTS(f, -1, names, &existing_id, media::Action::Append);
		if (new_id != -1) {
			media->changed_by_myself_ = true;
		}
	}
	
	if (new_id != -1) {
		media->Save();
		UpdateCurrentCategoryItems(new_id);
	} else {
		UpdateCurrentCategoryItems(existing_id);
	}
}

void MediaDialog::ButtonClicked(QAbstractButton *btn)
{
	if (btn == button_box_->button(QDialogButtonBox::Save)) {
		mtl_warn("Saved: %s", Save() ? "yes" : "no");
	} else if (btn == button_box_->button(QDialogButtonBox::Close)) {
		this->close();
	}
}

media::Field MediaDialog::GetCurrentCategory() const
{
	return (media::Field)(u8)categories_cb_->currentData().toInt();
}

i64 MediaDialog::GetCurrentID() const
{
	QVariant v = category_items_cb_->currentData();
	return v.isValid() ? v.toLongLong() : -1;
}

bool MediaDialog::Init()
{
	Media *media = app_->media();
	if (!media->loaded()) {
		media->Reload();
	}
	
	QFormLayout *layout = new QFormLayout();
	setLayout(layout);
	
	categories_cb_ = new QComboBox();
	categories_cb_->addItem(tr("Actors"), (u8)media::Field::Actors);
	categories_cb_->addItem(tr("Directors"), (u8)media::Field::Directors);
	categories_cb_->addItem(tr("Writers"), (u8)media::Field::Writers);
	categories_cb_->addItem(tr("Genres"), (u8)media::Field::Genres);
	categories_cb_->addItem(tr("Subgenres"), (u8)media::Field::Subgenres);
	categories_cb_->addItem(tr("Countries"), (u8)media::Field::Countries);
	connect(categories_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), [=] {
		UpdateCurrentCategoryItems();
	});
	layout->addRow(tr("Category:"), categories_cb_);
	
	cint a_size = fontMetrics().boundingRect("a").width();
	cint fixed_width = a_size * 40;
	
	category_items_cb_ = new QComboBox();
	layout->addRow(tr("Pick:"), category_items_cb_);
	connect(category_items_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), [=] {
		UpdateNamesFields();
	});
	
	category_items_cb_->setMinimumWidth(fixed_width);
	
	orig_lang_le_ = new TextField();
	orig_lang_le_->setFixedWidth(fixed_width);
	layout->addRow(tr("In original language:"), orig_lang_le_);

	native_lang_le_ = new TextField();
	native_lang_le_->setFixedWidth(fixed_width);
	layout->addRow(tr("In my language:"), native_lang_le_);
	
	button_box_ = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close);
	/// addButton() takes ownership
	add_as_new_btn_ = button_box_->addButton(tr("Add as new item"), QDialogButtonBox::NoRole);
	add_as_new_btn_->setIcon(QIcon::fromTheme(QLatin1String("contact-new")));
	connect(add_as_new_btn_, &QPushButton::clicked, [=] {
		AddNewItem();
	});
	
	connect(button_box_, &QDialogButtonBox::clicked, this, &MediaDialog::ButtonClicked);
	layout->addWidget(button_box_);
	UpdateCurrentCategoryItems();
	
	return true;
}

bool MediaDialog::Save()
{
	const media::Field f = GetCurrentCategory();
	QVector<QString> names = {
		orig_lang_le_->text().trimmed(),
		native_lang_le_->text().trimmed()
	};
	
	bool should_be_saved = false;
	for (const auto &next: names)
	{
		if (!next.isEmpty())
		{
			should_be_saved = true;
			break;
		}
	}
	
	Media *media = app_->media();
	ci64 ID = GetCurrentID();
	if (ID != -1 && should_be_saved) {
		{
			auto g = media->guard();
			media->SetNTS(f, ID, names);
		}
		SetCurrentByData(category_items_cb_, ID, &names);
	}
	return media->Save();
}

void MediaDialog::SetCurrentByData(QComboBox *cb, ci64 ID,
	const QVector<QString> *names)
{
	if (ID == -1)
		return;
	
	QString new_val;
	if (names != nullptr) {
		if (names->size() > 0 && !names->at(0).isEmpty())
			new_val = names->at(0);
		else if (names->size() > 1 && !names->at(1).isEmpty())
			new_val = names->at(1);
	}
	
	const int count = cb->count();
	for (int i = 0; i < count; i++)
	{
		QVariant v = cb->itemData(i);
		if (v.isValid() && v.toLongLong() == ID) {
			cb->setCurrentIndex(i);
			if(names != nullptr)
				cb->setItemText(i, new_val);
			return;
		}
	}
}

void MediaDialog::UpdateCurrentCategoryItems(ci64 select_id)
{
	category_items_cb_->clear();
	const media::Field category = GetCurrentCategory();
	{
		Media *media = app_->media();
		auto g = media->guard();
		media->FillInNTS(category_items_cb_, category);
	}
	
	SetCurrentByData(category_items_cb_, select_id);
	UpdateNamesFields();
}

void MediaDialog::UpdateNamesFields()
{
	i64 ID = GetCurrentID();
	Media *media = app_->media();
	QString v1, v2;
	if (ID != -1)
	{
		if(!media->TryLock())
			return;
		QVector<QString> vec = media->GetNTS(GetCurrentCategory(), ID);
		media->Unlock();
		if (vec.size() > 0)
			v1 = vec[0];
		if (vec.size() > 1)
			v2 = vec[1];
	}
	
	orig_lang_le_->setText(v1);
	native_lang_le_->setText(v2);
}

}
