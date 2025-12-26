#pragma once

#include "../ByteArray.hpp"
#include "decl.hxx"
#include "../decl.hxx"
#include "../media.hxx"
#include "../io/decl.hxx"

#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QPlainTextEdit>

namespace cornus::gui {
class AssignedPane;

class AvailPane: public QWidget {
public:
	AvailPane(AttrsDialog *ad, const media::Field f);
	virtual ~AvailPane();
	
	void asp(AssignedPane *p) { asp_ = p; }
	media::Field category() const { return category_; }
	QComboBox* cb() const { return cb_; }
	void Init();
	
private:
	NO_ASSIGN_COPY_MOVE(AvailPane);
	
	media::Field category_ = media::Field::None;
	QComboBox *cb_ = nullptr;
	AssignedPane *asp_ = nullptr;
	AttrsDialog *attrs_dialog_ = nullptr;
};

class AssignedPane: public QWidget {
public:
	AssignedPane(AttrsDialog *ad);
	virtual ~AssignedPane();
	
	void AddItemFromAvp();
	void avp(AvailPane *p) { avp_ = p; }
	QComboBox* cb() const { return cb_; }
	void Init();
	void WriteTo(ByteArray &buf);
	
private:
	NO_ASSIGN_COPY_MOVE(AssignedPane);
	
	QComboBox *cb_ = nullptr;
	AttrsDialog *attrs_dialog_ = nullptr;
	AvailPane *avp_ = nullptr;
};


class AttrsDialog: public QDialog {
public:
	AttrsDialog(App *app, io::File *file);
	virtual ~AttrsDialog();
	
	void Accept();
	int fixed_width() const { return fixed_width_; }
	App* app() const { return app_; }

private:
	NO_ASSIGN_COPY_MOVE(AttrsDialog);
	
	void CreateRow(QFormLayout *fl, AvailPane **avp, AssignedPane **asp, const media::Field f);
	void Init();
	void SaveAssignedAttrs();
	void SyncWidgetsWithFile();
	
	App *app_ = nullptr;
	io::File *file_ = nullptr;
	
	AvailPane *actors_avp_ = nullptr, *directors_avp_ = nullptr,
		*writers_avp_ = nullptr, *genres_avp_ = nullptr,
		*subgenres_avp_ = nullptr, *countries_avp_ = nullptr,
		*rip_avp_ = nullptr, *video_codec_avp_ = nullptr;
	
	AssignedPane *actors_asp_ = nullptr, *directors_asp_ = nullptr,
		*writers_asp_ = nullptr, *genres_asp_ = nullptr,
		*subgenres_asp_ = nullptr, *countries_asp_ = nullptr,
		*rip_asp_ = nullptr, *video_codec_asp_ = nullptr;
	
	TextField *year_started_tf_ = nullptr, *year_ended_tf_ = nullptr,
	*bit_depth_tf_ = nullptr, *resolution_w_tf_ = nullptr,
	*resolution_h_tf_ = nullptr, *fps_tf_ = nullptr,
	*month_started_tf_ = nullptr, *day_started_tf_ = nullptr;
	int fixed_width_ = -1;
	
	QPlainTextEdit *comment_area_ = nullptr;
	
	ByteArray was_;
};
}
