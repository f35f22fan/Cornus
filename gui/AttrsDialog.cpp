#include "AttrsDialog.hpp"

#include "../App.hpp"
#include "../ByteArray.hpp"
#include "../Media.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"

#include <QBoxLayout>
#include <QLabel>

namespace cornus::gui {

AvailPane::AvailPane(AttrsDialog *ad, const media::Field f):
category_(f), attrs_dialog_(ad)
{}

AvailPane::~AvailPane()
{
	for (QWidget *next: widgets_)
		delete next;
}

void AvailPane::Init()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(layout);
	QString label;
	switch (category_) {
	case media::Field::Actors: { label = tr("Actors:"); break; }
	case media::Field::Directors: { label = tr("Directors:"); break; }
	case media::Field::Writers: { label = tr("Writers:"); break; }
	case media::Field::Genres: { label = tr("Genres:"); break; }
	case media::Field::Subgenres: { label = tr("Subgenres:"); break; }
	case media::Field::Countries: { label = tr("Countries:"); break; }
	default: {
		mtl_trace();
	}
	}
	QLabel *l = new QLabel(label);
	widgets_.append(l);
	layout->addWidget(l);
	cb_ = new QComboBox();
	cb_->setFixedWidth(attrs_dialog_->fixed_width());
	layout->addWidget(cb_);
	widgets_.append(cb_);
	
	QPushButton *btn = new QPushButton(tr("Add →"));
	connect(btn, &QPushButton::clicked, [=] { asp_->AddItemFromAvp(); });
	widgets_.append(btn);
	layout->addWidget(btn);
	
	Media *media = attrs_dialog_->app()->media();
	{
		auto g = media->guard();
		media->FillInNTS(cb_, category_);
	}
}

AssignedPane::AssignedPane(AttrsDialog *ad):
attrs_dialog_(ad)
{}

AssignedPane::~AssignedPane()
{
	for (QWidget *next: widgets_)
		delete next;
}

void AssignedPane::AddItemFromAvp()
{
	QComboBox *rhs = avp_->cb();
	if (rhs->count() == 0)
		return;
	QVariant data = rhs->currentData();
	const int data_index = cb_->findData(data);
	if (data_index == -1) {
		cb_->addItem(rhs->currentText(), data);
		cb_->model()->sort(0, Qt::AscendingOrder);
		cb_->setCurrentIndex(cb_->findData(data));
	} else {
		cb_->setCurrentIndex(data_index);
		mtl_info("data: %ld already exists", i64(data.toLongLong()));
	}
}

void AssignedPane::Init()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(layout);
	cb_ = new QComboBox();
	cb_->setFixedWidth(attrs_dialog_->fixed_width());
	layout->addWidget(cb_);
	widgets_.append(cb_);
	
	QPushButton *rem_btn = new QPushButton();
	connect(rem_btn, &QPushButton::clicked, [=] {
		const int index = cb_->currentIndex();
		if (index != -1)
			cb_->removeItem(index);
	});
	rem_btn->setToolTip(tr("Remove item from list"));
	rem_btn->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
	layout->addWidget(rem_btn);
	widgets_.append(rem_btn);
}

void AssignedPane::WriteTo(ByteArray &ba)
{
	const media::Field f = avp_->category();
	attrs_dialog_->app()->media()->WriteTo(ba, cb_, f);
}

AttrsDialog::AttrsDialog(App *app, io::File *file):
QDialog(app), app_(app), file_(file)
{
	Q_UNUSED(year_);
	Q_UNUSED(year_end_);
	Init();
	setModal(true);
	setWindowTitle(tr("File Metadata"));
	adjustSize();
	exec();
}

AttrsDialog::~AttrsDialog()
{
	SaveAssignedAttrs();
	delete file_;
	for (QWidget *next: widgets_)
		delete next;
}

void AttrsDialog::CreateRow(QFormLayout *fl, AvailPane **avp, AssignedPane **asp,
	const media::Field f)
{
	*avp = new AvailPane(this, f);
	*asp = new AssignedPane(this);
	fl->addRow(*avp, *asp);
	(*asp)->avp(*avp);
	(*avp)->asp(*asp);
	(*asp)->Init();
	(*avp)->Init();
	widgets_.append(*avp);
	widgets_.append(*asp);
}

void AttrsDialog::Init()
{
	Media *media = app_->media();
	if (!media->loaded_)
		cornus::media::Reload(app_);
	
	fixed_width_ = fontMetrics().boundingRect('a').width() * 30;
	
	QFormLayout *fl = new QFormLayout();
	setLayout(fl);
	{
		QWidget *w = new QWidget();
		QBoxLayout *horiz = new QBoxLayout(QBoxLayout::LeftToRight);
		w->setLayout(horiz);
		QLabel *label = new QLabel(tr("File:"));
		widgets_.append(label);
		horiz->addWidget(label);
		
		QLineEdit *text_field = new QLineEdit();
		widgets_.append(text_field);
		text_field->setReadOnly(true);
		text_field->setText(file_->build_full_path());
		horiz->addWidget(text_field);
		
		fl->addRow(w);
		widgets_.append(w);
	}
	{
		QLabel *assigned = new QLabel(tr("Assigned to the file ↓"));
		widgets_.append(assigned);
		fl->addRow(tr("↓ Available items |"), assigned);
	}
	CreateRow(fl, &actors_avp_, &actors_asp_, media::Field::Actors);
	CreateRow(fl, &directors_avp_, &directors_asp_, media::Field::Directors);
	CreateRow(fl, &writers_avp_, &writers_asp_, media::Field::Writers);
	CreateRow(fl, &genres_avp_, &genres_asp_, media::Field::Genres);
	CreateRow(fl, &subgenres_avp_, &subgenres_asp_, media::Field::Subgenres);
	CreateRow(fl, &countries_avp_, &countries_asp_, media::Field::Countries);
	
	SyncWidgetsToFile();
}

void AttrsDialog::SaveAssignedAttrs()
{
	ByteArray ba;
	
	Media *media = app_->media();
	i32 magic_num = media->GetMagicNumber();
	ba.add_i32(magic_num);
	actors_asp_->WriteTo(ba);
	directors_asp_->WriteTo(ba);
	writers_asp_->WriteTo(ba);
	genres_asp_->WriteTo(ba);
	subgenres_asp_->WriteTo(ba);
	countries_asp_->WriteTo(ba);
	
	file_->ext_attrs().insert(media::XAttrName, ba);
	io::SetXAttr(file_->build_full_path(), media::XAttrName, ba);
}

void AttrsDialog::SyncWidgetsToFile()
{
	if (!file_->has_media_attrs())
		return;
	ByteArray ba = file_->media_attrs();
	CHECK_TRUE_VOID((ba.size() >= 4));
	const i32 magic = ba.next_i32();
	Media *media = app_->media();
	const i32 media_magic = media->GetMagicNumber();
	if (media_magic != magic) {
		mtl_warn("Different magic numbers, file: %d vs media: %d", magic, media_magic);
		return;
	}
	
	while (ba.has_more())
	{
		QComboBox *cb = nullptr;
		const media::Field f = (media::Field)ba.next_u8();
		switch (f) {
		case media::Field::Actors: cb = actors_asp_->cb(); break;
		case media::Field::Directors: cb = directors_asp_->cb(); break;
		case media::Field::Writers: cb = writers_asp_->cb(); break;
		case media::Field::Genres: cb = genres_asp_->cb(); break;
		case media::Field::Subgenres: cb = subgenres_asp_->cb(); break;
		case media::Field::Countries: cb = countries_asp_->cb(); break;
		default: {
			mtl_trace();
		}
		}

		if (cb != nullptr)
			media->ApplyTo(cb, ba, f);
	}
}

}


