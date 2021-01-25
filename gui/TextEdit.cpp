#include "TextEdit.hpp"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../io/File.hpp"
#include "../io/io.hh"
#include "Hiliter.hpp"

#include <QKeyEvent>
#include <QVector>

namespace cornus::gui {

const QVector<QString> Extensions_C_CPP = {
	QLatin1String("cpp"), QLatin1String("hpp"),
	QLatin1String("c"), QLatin1String("h"),
	QLatin1String("cc"), QLatin1String("hh"),
	QLatin1String("hxx")
};


TextEdit::TextEdit(App *app): app_(app)
{
	setEnabled(true);
	setReadOnly(false);
	setAcceptRichText(false);
	setAutoFormatting(QTextEdit::AutoNone);
	hiliter_ = new Hiliter(document());
	
	QFont font;
	font.setFamily("Hack");
	font.setFixedPitch(true);
	font.setPointSize(13);
	setFont(font);
	
	QFontMetrics fm(font);
	const int tab_w = fm.boundingRect(" ").width() * 4;
	setTabStopDistance(tab_w);
}

TextEdit::~TextEdit() {
}

bool TextEdit::Display(io::File *cloned_file)
{
	AutoDelete ad(cloned_file);
	
	if (cloned_file->is_dir_or_so())
		return false;
	
	ByteArray buf;
	const i64 MAX = 1024 * 1024; // 1 MiB
	full_path_ = cloned_file->build_full_path();
	
	if (io::ReadFile(full_path_, buf, MAX) != io::Err::Ok) {
		mtl_warn("Read %ld bytes", buf.size());
		return false;
	}
	
	hilite_mode_ = GetHiliteMode(buf, cloned_file);
	hiliter_->SwitchTo(hilite_mode_);
	setReadOnly(hilite_mode_ == HiliteMode::None);
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	setText(s);
	moveCursor(QTextCursor::Start);
	
	return true;
}

HiliteMode
TextEdit::GetHiliteMode(const ByteArray &buf, io::File *file)
{
	auto ext = file->cache().ext;
	
	if (ext.isEmpty()) {
		ExecInfo exec = {};
		app_->TestExecBuf(buf.constData(), buf.size(), exec);
		
		if (exec.is_script_bash() || exec.is_script_sh()) {
			return HiliteMode::SH;
		} else if (exec.is_script_python()) {
			return HiliteMode::Python;
		}
		
		return HiliteMode::None;
	}
	
	if (ext == QLatin1String("txt"))
		return HiliteMode::PlainText;
	
	for (const auto &next: Extensions_C_CPP) {
		if (next == ext)
			return HiliteMode::C_CPP;
	}
	
	return HiliteMode::None;
}

void TextEdit::keyPressEvent(QKeyEvent *evt)
{
	QTextEdit::keyPressEvent(evt);
	
	const auto key = evt->key();
	const bool ctrl = evt->modifiers() & Qt::ControlModifier;
	
	if (ctrl) {
		if (key == Qt::Key_S) {
			if (Save()) {
				clear();
				app_->HideTextEditor();
			}
		}
		
		return;
	}
	
	if (key == Qt::Key_Escape) {
		clear();
		app_->HideTextEditor();
	}
}

bool TextEdit::Save()
{
	if (hilite_mode_ == HiliteMode::None)
		return false;
	
	auto ba = toPlainText().toLocal8Bit();

	return io::WriteToFile(full_path_, ba.data(), ba.size()) == io::Err::Ok;
}

}
