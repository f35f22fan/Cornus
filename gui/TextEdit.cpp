#include "TextEdit.hpp"
#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../ExecInfo.hpp"
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
	//setAcceptRichText(false);
//	setAutoFormatting(QTextEdit::AutoNone);
//	setLineWrapMode(QTextEdit::NoWrap);
	hiliter_ = new Hiliter(document());
	
	QFont font;
	font.setFamily(QLatin1String("Hack"));
	font.setFixedPitch(true);
	font.setPointSize(13);
	setFont(font);
	
	QFontMetrics fm(font);
	const int tab_w = fm.horizontalAdvance(QChar(' ')) * 4;
	setTabStopDistance(tab_w);
	
	{ // visualize whitespaces
		QTextOption option = document()->defaultTextOption();
		option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
		document()->setDefaultTextOption(option);
	}
}

TextEdit::~TextEdit() {
}

bool TextEdit::Display(io::File *cloned_file)
{
	clear();
	
	AutoDelete ad(cloned_file);

	if (cloned_file->is_dir_or_so()) {
		return false;
	}
	
	const i64 MAX = 1024 * 1024; // 1 MiB
	full_path_ = cloned_file->build_full_path();
	ByteArray buf;
	RET_IF(io::ReadFile(full_path_, buf, PrintErrors::Yes, MAX), false, false);
	
	hilite_mode_ = GetHiliteMode(buf, cloned_file);
	hiliter_->SwitchTo(hilite_mode_);
	setReadOnly(hilite_mode_ == HiliteMode::None);
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	setPlainText(s);
	moveCursor(QTextCursor::Start);
	
	return true;
}

HiliteMode
TextEdit::GetHiliteMode(const ByteArray &buf, io::File *file)
{
	auto ext = file->cache().ext;
	if (ext.isEmpty()) {
		ExecInfo exec;
		app_->TestExecBuf(buf.constData(), buf.size(), exec);
		
		if (exec.is_shell_script()) {
			return HiliteMode::ShellScript;
		}
		
		const QString mime = app_->QueryMimeType(file->build_full_path());
		if (mime.startsWith(QLatin1String("text/")))
			return HiliteMode::PlainText;
		
		return HiliteMode::None;
	}
	
	if (ext == QLatin1String("txt"))
		return HiliteMode::PlainText;
	if (ext == QLatin1String("asm"))
		return HiliteMode::Assembly_NASM;
	
	for (const auto &next: Extensions_C_CPP) {
		if (next == ext)
			return HiliteMode::C_CPP;
	}
	
	if (ext == QLatin1String("sh") || ext == QLatin1String("run"))
		return HiliteMode::ShellScript;
	
	if (ext == QLatin1String("bashrc") || ext == QLatin1String("profile"))
		return HiliteMode::ShellScript;
	
	if (ext == QLatin1String("desktop")) {
		return HiliteMode::DesktopFile;
	}
	
	if (ext == QLatin1String("js") || ext == QLatin1String("html") ||
		ext == QLatin1String("css") || ext == QLatin1String("php") ||
		ext == QLatin1String("py") || ext == QLatin1String("java") ||
		ext == QLatin1String("pl") || ext == QLatin1String("xml") ||
		ext == QLatin1String("rs") || ext == QLatin1String("go") ||
		ext == QLatin1String("md") || ext == QLatin1String("qrc"))
	{
		return HiliteMode::PlainText;
	}
	
	const QString mime = app_->QueryMimeType(file->build_full_path());
	if (mime.startsWith(QLatin1String("text/")))
		return HiliteMode::PlainText;
	return HiliteMode::None;
}

void TextEdit::keyPressEvent(QKeyEvent *evt)
{
	QPlainTextEdit::keyPressEvent(evt);
	
	const auto key = evt->key();
	const bool ctrl = evt->modifiers() & Qt::ControlModifier;
	
	if (ctrl) {
		if (key == Qt::Key_S) {
			if (Save()) {
				app_->HideTextEditor();
			}
		}
		
		return;
	}
	
	if (key == Qt::Key_Escape) {
		app_->HideTextEditor();
	}
}

bool TextEdit::Save()
{
	if (hilite_mode_ == HiliteMode::None)
		return false;
	
	auto ba = toPlainText().toLocal8Bit();
	return (io::WriteToFile(full_path_, ba.data(), ba.size()) == 0);
}

}
