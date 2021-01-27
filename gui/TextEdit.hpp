#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"

#include <QPlainTextEdit>

namespace cornus::gui {

class TextEdit: public QPlainTextEdit {
public:
	TextEdit(App *app);
	virtual ~TextEdit();
	
	bool Display(io::File *cloned_file);
	
protected:
	virtual void keyPressEvent(QKeyEvent *event) override;
	
private:
	HiliteMode GetHiliteMode(const cornus::ByteArray &buf, io::File *file);
	bool Save();
	
	App *app_ = nullptr;
	HiliteMode hilite_mode_ = HiliteMode::None;
	QString full_path_;
	gui::Hiliter *hiliter_ = nullptr;
};

}
