#include "AttrsDialog.hpp"

#include "../App.hpp"
#include "../Media.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "TextField.hpp"

#include <QBoxLayout>
#include <QLabel>

namespace cornus::gui {

AvailPane::AvailPane(AttrsDialog *ad, const media::Field f):
category_(f), attrs_dialog_(ad)
{}

AvailPane::~AvailPane()
{}

void AvailPane::Init()
{
	setContentsMargins(0, 0, 0, 0);
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	///layout->setSpacing(0);
	setLayout(layout);
	QString label;
	switch (category_) {
	case media::Field::Actors: { label = tr("Actors:"); break; }
	case media::Field::Directors: { label = tr("Directors:"); break; }
	case media::Field::Writers: { label = tr("Writers:"); break; }
	case media::Field::Genres: { label = tr("Genres:"); break; }
	case media::Field::Subgenres: { label = tr("Subgenres:"); break; }
	case media::Field::Countries: { label = tr("Countries:"); break; }
	case media::Field::Rip: { label = tr("Rip:"); break; }
	case media::Field::VideoCodec: { label = tr("Video Codec"); break; }
	default: {
		mtl_trace();
	}
	}
	QLabel *l = new QLabel(label);
	layout->addWidget(l);
	cb_ = new QComboBox();
	cb_->setFixedWidth(attrs_dialog_->fixed_width());
	layout->addWidget(cb_);
	
	QPushButton *btn = new QPushButton(tr("Add →"));
	connect(btn, &QPushButton::clicked, [=] { asp_->AddItemFromAvp(); });
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
{}

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
	}
}

void AssignedPane::Init()
{
	setContentsMargins(0, 0, 0, 0);
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	///layout->setSpacing(0);
	setLayout(layout);
	cb_ = new QComboBox();
	cb_->setFixedWidth(attrs_dialog_->fixed_width());
	layout->addWidget(cb_);
	
	QPushButton *rem_btn = new QPushButton();
	connect(rem_btn, &QPushButton::clicked, [=] {
		const int index = cb_->currentIndex();
		if (index != -1)
			cb_->removeItem(index);
	});
	rem_btn->setToolTip(tr("Remove item from list"));
	rem_btn->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
	layout->addWidget(rem_btn);
}

void AssignedPane::WriteTo(ByteArray &ba)
{
	const media::Field f = avp_->category();
	attrs_dialog_->app()->media()->WriteTo(ba, cb_, f);
}

AttrsDialog::AttrsDialog(App *app, io::File *file):
QDialog(app), app_(app), file_(file)
{
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
}

void AttrsDialog::Init()
{
	Media *media = app_->media();
	if (!media->loaded())
		cornus::media::Reload(app_);
	
	const int a_size = fontMetrics().boundingRect('a').width();
	fixed_width_ = a_size * 30;
	
	QFormLayout *fl = new QFormLayout();
	fl->setContentsMargins(0, 0, 0, 0);
	fl->setSpacing(0);
	setLayout(fl);
	{
		QWidget *w = new QWidget();
		QBoxLayout *horiz = new QBoxLayout(QBoxLayout::LeftToRight);
		w->setLayout(horiz);
		QLabel *label = new QLabel(tr("File:"));
		horiz->addWidget(label);
		
		TextField *text_field = new TextField();
		text_field->setReadOnly(true);
		text_field->setText(file_->build_full_path());
		horiz->addWidget(text_field);
		
		fl->addRow(w);
	}
	{
		QLabel *assigned = new QLabel(tr("Assigned to the file ↓"));
		fl->addRow(tr("↓ Available items |"), assigned);
	}
	CreateRow(fl, &actors_avp_, &actors_asp_, media::Field::Actors);
	CreateRow(fl, &directors_avp_, &directors_asp_, media::Field::Directors);
	CreateRow(fl, &writers_avp_, &writers_asp_, media::Field::Writers);
	CreateRow(fl, &genres_avp_, &genres_asp_, media::Field::Genres);
	CreateRow(fl, &subgenres_avp_, &subgenres_asp_, media::Field::Subgenres);
	CreateRow(fl, &countries_avp_, &countries_asp_, media::Field::Countries);
	CreateRow(fl, &rip_avp_, &rip_asp_, media::Field::Rip);
	CreateRow(fl, &video_codec_avp_, &video_codec_asp_, media::Field::VideoCodec);
	{
		QWidget *pane = new QWidget();
		
		QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
		pane->setLayout(layout);
		QString tooltip = tr("Video resolution, e.g. 1920x1080");
		
		const int fixed_w = a_size * 8;
		const int small_w = a_size * 6;
		resolution_w_tf_ = new TextField();
		resolution_w_tf_->setToolTip(tooltip);
		resolution_w_tf_->setPlaceholderText(tr("width"));
		resolution_w_tf_->setFixedWidth(fixed_w);
		resolution_h_tf_ = new TextField();
		resolution_h_tf_->setToolTip(tooltip);
		resolution_h_tf_->setPlaceholderText(tr("height"));
		resolution_h_tf_->setFixedWidth(fixed_w);
		bit_depth_tf_ = new TextField();
		bit_depth_tf_->setPlaceholderText(tr("bits"));
		bit_depth_tf_->setFixedWidth(small_w);
		bit_depth_tf_->setToolTip(tr("Video bit depth, usually 8, 10 or 12 bits"));
		fps_tf_ = new TextField();
		fps_tf_->setFixedWidth(small_w);
		fps_tf_->setToolTip(tr("FPS - frames per second"));
		fps_tf_->setPlaceholderText("FPS");
		
		layout->addWidget(resolution_w_tf_);
		layout->addWidget(new QLabel(QLatin1String("x")));
		layout->addWidget(resolution_h_tf_);
		layout->addWidget(bit_depth_tf_);
		layout->addWidget(fps_tf_);
		layout->addStretch(2);
		
		fl->addRow(tr("Video resolution, bit depth, FPS:"), pane);
	}
	
	{
		QWidget *pane = new QWidget();
		
		QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
		pane->setLayout(layout);
		
		const int fixed_w = a_size * 8;
		year_started_tf_ = new TextField();
		year_started_tf_->setFixedWidth(fixed_w);
		year_ended_tf_ = new TextField();
		year_ended_tf_->setFixedWidth(fixed_w);
		
		layout->addWidget(year_started_tf_);
		layout->addWidget(new QLabel(QLatin1String(" - ")));
		layout->addWidget(year_ended_tf_);
		layout->addStretch(2);
		
		fl->addRow(tr("Released in (years):"), pane);
	}
	
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
	rip_asp_->WriteTo(ba);
	video_codec_asp_->WriteTo(ba);
	
	bool ok;
	{
		QString s = year_started_tf_->text().trimmed();
		i16 n = s.toInt(&ok);
		if (ok) {
			ba.add_u8((u8)media::Field::YearStarted);
			ba.add_i16(n);
		}
	}
	{
		QString s = year_ended_tf_->text().trimmed();
		int n = s.toInt(&ok);
		if (ok) {
			ba.add_u8((u8)media::Field::YearEnded);
			ba.add_i16(n);
		}
	}
	{
		QString s = bit_depth_tf_->text().trimmed();
		int n = s.toInt(&ok);
		if (ok) {
			ba.add_u8((u8)media::Field::VideoCodecBitDepth);
			ba.add_i16(n);
		}
	}
	
	{
		QString s = fps_tf_->text().trimmed();
		int n = s.toInt(&ok);
		if (ok) {
			ba.add_u8((u8)media::Field::FPS);
			ba.add_i16(n);
		}
	}
	
	{
		QString s = resolution_w_tf_->text().trimmed();
		int w = s.toInt(&ok);
		
		if (ok) {
			s = resolution_h_tf_->text().trimmed();
			int h = s.toInt(&ok);
			if (ok) {
				ba.add_u8((u8)media::Field::VideoResolution);
				ba.add_i32(w);
				ba.add_i32(h);
			}
		}
	}
	
	if (ba == was_) {
///		mtl_info("Nothing changed, returning.");
		return;
	}
	
///	mtl_info("%ld vs %ld", was_.size(), ba.size());
	
	auto &h = file_->ext_attrs();
	if (ba.size() > sizeof magic_num) {
		h.insert(media::XAttrName, ba);
		io::SetXAttr(file_->build_full_path(), media::XAttrName, ba);
	} else {
		if (h.contains(media::XAttrName)) {
			io::RemoveXAttr(file_->build_full_path(), media::XAttrName);
			h.remove(media::XAttrName);
		}
	}
}

void AttrsDialog::SyncWidgetsToFile()
{
	if (!file_->has_media_attrs())
		return;
	was_ = file_->media_attrs();
	ByteArray &ba = was_;
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
		case media::Field::Rip: cb = rip_asp_->cb(); break;
		case media::Field::VideoCodec: cb = video_codec_asp_->cb(); break;
		case media::Field::YearStarted: {
			year_started_tf_->setText(QString::number(ba.next_i16()));
			break;
		}
		case media::Field::YearEnded: {
			year_ended_tf_->setText(QString::number(ba.next_i16()));
			break;
		}
		case media::Field::VideoCodecBitDepth: {
			bit_depth_tf_->setText(QString::number(ba.next_i16()));
			break;
		}
		case media::Field::VideoResolution: {
			resolution_w_tf_->setText(QString::number(ba.next_i32()));
			resolution_h_tf_->setText(QString::number(ba.next_i32()));
			break;
		}
		case media::Field::FPS: {
			fps_tf_->setText(QString::number(ba.next_i16()));
			break;
		}
		default:{
			mtl_trace();
		}
		}

		if (cb != nullptr)
			media->ApplyTo(cb, ba, f);
	}
}

}
