#include "TextEdit.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../ExecInfo.hpp"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "../io/io.hh"
#include "../io/SaveFile.hpp"
#include "Hiliter.hpp"
#include "Tab.hpp"

#include <QKeyEvent>
#include <QVector>

namespace cornus::gui {

const QVector<QString> Extensions_C_CPP = {
	QLatin1String("cpp"), QLatin1String("hpp"),
	QLatin1String("c"), QLatin1String("h"),
	QLatin1String("cc"), QLatin1String("hh"),
	QLatin1String("hxx")
};

TextEdit::TextEdit(Tab *tab): tab_(tab)
{
	app_ = tab_->app();
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
	if (cloned_file->is_dir_or_so())
	{
		return false;
	}
	
	was_selected_ = cloned_file->selected();
	filename_ = cloned_file->name();
	full_path_ = cloned_file->build_full_path();
	io::ReadParams read_params = {};
	read_params.print_errors = PrintErrors::Yes;
	read_params.read_max = 1024 * 1024 * 5; // 5 MiB
	
	ByteArray buf;
	MTL_CHECK(io::ReadFile(full_path_, buf, read_params));
	
	hilite_mode_ = GetHiliteMode(buf, cloned_file);
	hiliter_->SwitchTo(hilite_mode_);
	
	QString s;
	const bool read_only = (hilite_mode_ == HiliteMode::None);
	setReadOnly(read_only);
	if (read_only)
	{
		// fromLatin1() instead of fromLocal8Bit() to avoid some rare crashes
		// when trying to convert to string blobs that aren't strings:
		s = QString::fromLatin1(buf.data(), buf.size());
	} else {
		s = QString::fromLocal8Bit(buf.data(), buf.size());
	}
	
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
	
	if (ctrl)
	{
		if (key == Qt::Key_S) {
			if (Save()) {
				app_->SetTopLevel(TopLevel::Browser);
			}
		}
		return;
	}
	
	if (key == Qt::Key_Escape) {
		app_->SetTopLevel(TopLevel::Browser);
	}
}

bool TextEdit::Save()
{
	if (hilite_mode_ == HiliteMode::None)
		return false;
	
	io::SaveFile save_file(full_path_);
	auto ba = toPlainText().toLocal8Bit();
	if (io::WriteToFile(save_file.GetPathToWorkWith(), ba.data(), ba.size()))
	{
		if (!save_file.Commit(PrintErrors::Yes))
			return false;
		
		if (was_selected_ == Selected::Yes)
		{
			QVector<QString> names = {filename_};
			io::Files &files = tab_->view_files();
			files.SelectFilenamesLater(names, SameDir::Yes);
		}
		
		return true;
	}

	save_file.CommitCancelled();

	return false;
}

}
