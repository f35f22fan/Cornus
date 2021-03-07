#include "Hiliter.hpp"

namespace cornus::gui {

Hiliter::Hiliter(QTextDocument *parent): QSyntaxHighlighter(parent)
{
	{
		formats_.keyword.setForeground(Qt::darkBlue);
		formats_.keyword.setFontWeight(QFont::Bold);
		
		formats_.klass.setFontWeight(QFont::Bold);
		formats_.klass.setForeground(Qt::darkMagenta);
		
		formats_.single_line_comment.setForeground(QColor(128, 128, 128));
		formats_.quotation.setForeground(Qt::darkGreen);
		
		formats_.function.setFontItalic(true);
		formats_.function.setForeground(Qt::blue);
	}
	
	multiline_comment_format_.setForeground(Qt::blue);
	comment_start_expression_ = QRegularExpression(QLatin1String("/\\*"));
	comment_end_expression_ = QRegularExpression(QLatin1String("\\*/"));
}

void Hiliter::highlightBlock(const QString &text)
{
	for (const HighlightingRule &rule : qAsConst(hiliting_rules_))
	{
		QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
		while (matchIterator.hasNext()) {
			QRegularExpressionMatch match = matchIterator.next();
			setFormat(match.capturedStart(), match.capturedLength(), rule.format);
		}
	}
	
	setCurrentBlockState(0);
	
	if (mode_ == HiliteMode::C_CPP) {
		
		int startIndex = 0;
		if (previousBlockState() != 1)
			startIndex = text.indexOf(comment_start_expression_);
		
		while (startIndex >= 0)
		{
			QRegularExpressionMatch match = comment_end_expression_.match(text, startIndex);
			int endIndex = match.capturedStart();
			int commentLength = 0;
			if (endIndex == -1) {
				setCurrentBlockState(1);
				commentLength = text.length() - startIndex;
			} else {
				commentLength = endIndex - startIndex
				+ match.capturedLength();
			}
			
			setFormat(startIndex, commentLength, multiline_comment_format_);
			startIndex = text.indexOf(comment_start_expression_, startIndex + commentLength);
		}
	}
}

void Hiliter::SetupC_CPP()
{
	HighlightingRule rule;
	{ /// keywords
		const QString keywords[] = {
		QLatin1String("\\bchar\\b"), QLatin1String("\\bclass\\b"), QLatin1String("\\bconst\\b"),
		QLatin1String("\\bdouble\\b"), QLatin1String("\\benum\\b"), QLatin1String("\\bexplicit\\b"),
		QLatin1String("\\bfriend\\b"), QLatin1String("\\binline\\b"), QLatin1String("\\bint\\b"),
		QLatin1String("\\blong\\b"), QLatin1String("\\bnamespace\\b"), QLatin1String("\\boperator\\b"),
		QLatin1String("\\bprivate\\b"), QLatin1String("\\bprotected\\b"), QLatin1String("\\bpublic\\b"),
		QLatin1String("\\bshort\\b"), QLatin1String("\\bsignals\\b"), QLatin1String("\\bsigned\\b"),
		QLatin1String("\\bslots\\b"), QLatin1String("\\bstatic\\b"), QLatin1String("\\bstruct\\b"),
		QLatin1String("\\btemplate\\b"), QLatin1String("\\btypedef\\b"), QLatin1String("\\btypename\\b"),
		QLatin1String("\\bunion\\b"), QLatin1String("\\bunsigned\\b"), QLatin1String("\\bvirtual\\b"),
		QLatin1String("\\bvoid\\b"), QLatin1String("\\bvolatile\\b"), QLatin1String("\\bbool\\b")
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = formats_.keyword;
			hiliting_rules_.append(rule);
		}
	}
	
	{ /// class
		rule.pattern = QRegularExpression(QLatin1String("\\bQ[A-Za-z]+\\b"));
		rule.format = formats_.klass;
		hiliting_rules_.append(rule);
	}
	
	{ /// single line comment
		rule.pattern = QRegularExpression(QLatin1String("//[^\n]*"));
		rule.format = formats_.single_line_comment;
		hiliting_rules_.append(rule);
	}
	
	{ /// quotation
		rule.pattern = QRegularExpression(QLatin1String("\".*\""));
		rule.format = formats_.quotation;
		hiliting_rules_.append(rule);
	}
	
	{ /// function
		rule.pattern = QRegularExpression(QLatin1String("\\b[A-Za-z0-9_]+(?=\\()"));
		rule.format = formats_.function;
		hiliting_rules_.append(rule);
	}
}

void Hiliter::SetupDesktopFile()
{
	HighlightingRule rule;
	{ /// keywords
		const auto pref = QLatin1String("\\b");
		const auto suf = QLatin1String("[\\s\\[\\w\\]\\@\\-]*=");
		QVector<QString> keywords = {
		QLatin1String("Version"), QLatin1String("Actions"),
		QLatin1String("Name"), QLatin1String("Exec"),
		QLatin1String("GenericName"), QLatin1String("Type"),
		QLatin1String("Comment"), QLatin1String("MimeType"),
		QLatin1String("X-DBUS-ServiceName"), QLatin1String("Categories"),
		QLatin1String("Actions"), QString("DBusActivatable"),
		QLatin1String("X-Ubuntu-Gettext-Domain"), QLatin1String("Keywords"),
		QLatin1String("StartupNotify"), QLatin1String("Implements"),
		QLatin1String("Terminal"), QLatin1String("Icon"),
		QLatin1String("TryExec"), QLatin1String("OnlyShowIn"),
		QLatin1String("NotShowIn"), QLatin1String("NoDisplay"),
		QLatin1String("InitialPreference"), QLatin1String("Hidden"),
		QLatin1String("DesktopNames"),
		QLatin1String("StartupWMClass"), QLatin1String("URL"),
		QLatin1String("PrefersNonDefaultGPU"),
		QLatin1String("X-KDE-ServiceTypes"), QLatin1String("X-Calligra-DefaultMimeTypes"),
		QLatin1String("X-KDE-NativeMimeType"), QLatin1String("X-DocPath"),
		QLatin1String("X-KDE-StartupNotify"), QLatin1String("X-DBUS-StartupType"),
		QLatin1String("X-KDE-PluginInfo-Version"),
		QLatin1String("X-GNOME-FullName"),
		QLatin1String("X-GNOME-UsesNotifications"),
		QLatin1String("X-Unity-IconBackgroundColor"),
		QLatin1String("X-Desktop-File-Install-Version"),
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pref + pattern + suf);
			rule.format = formats_.keyword;
			hiliting_rules_.append(rule);
		}
	}
	
	{ /// class
		rule.pattern = QRegularExpression(QLatin1String("\\[Desktop\\s+Entry\\]"));
		rule.format = formats_.klass;
		hiliting_rules_.append(rule);
		rule.pattern = QRegularExpression(QLatin1String("\\[Desktop\\s+Action.*\\]"));
		rule.format = formats_.klass;
		hiliting_rules_.append(rule);
	}
	
	{ /// single line comment
		rule.pattern = QRegularExpression(QLatin1String("\\#[^\n]*"));
		rule.format = formats_.single_line_comment;
		hiliting_rules_.append(rule);
	}
	
	{ /// quotation
		rule.pattern = QRegularExpression(QLatin1String("\".*\""));
		rule.format = formats_.quotation;
		hiliting_rules_.append(rule);
	}
}

void Hiliter::SetupPlainText() {}

void Hiliter::SetupShellScript()
{
	HighlightingRule rule;
	{ /// keywords
		const QString keywords[] = {
			QLatin1String("\\bexport\\b"), QLatin1String("\\bif\\b"),
			QLatin1String("\\belif\\b"), QLatin1String("\\bthen\\b"),
			QLatin1String("\\bfi\\b"), QLatin1String("\\belse\\b"),
			QLatin1String("\\bwhile\\b"), QLatin1String("\\bcat\\b"),
			QLatin1String("\\bdo\\b"),
			QLatin1String("\\bdone\\b"), QLatin1String("\\bfor\\b"),
			QLatin1String("\\buntil\\b"), QLatin1String("\\bbreak\\b"),
			QLatin1String("\\bcase\\b"), QLatin1String("\\besac\\b"),
			QLatin1String("\\bcontinue\\b"),
			QLatin1String("\\breturn\\b"),
			QLatin1String("\\beval\\b"),
			QLatin1String("\\balias\\b"),
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = formats_.keyword;
			hiliting_rules_.append(rule);
		}
	}
	
	{
		/// functions
		const QString keywords[] = {
		QLatin1String("\\becho\\b"), QLatin1String("\\bread\\b"),
		QLatin1String("\\bset\\b"), QLatin1String("\\bunset\\b"),
		QLatin1String("\\breadonly\\b"), QLatin1String("\\bshift\\b"),
		QLatin1String("\\btput\\b"), QLatin1String("\\btest\\b"),
		QLatin1String("\\bcat\\b"),
		QLatin1String("\\bexit\\b"),
		QLatin1String("\\btrap\\b"),
		QLatin1String("\\bwait\\b"), QLatin1String("\\beval\\b"),
		QLatin1String("\\bexec\\b"), QLatin1String("\\bulimit\\b"),
		QLatin1String("\\bumask\\b"),
		QLatin1String("\\bsource\\b"),
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = formats_.function;
			hiliting_rules_.append(rule);
		}
	}
	
	{ /// single line comment
		rule.pattern = QRegularExpression(QLatin1String("\\#[^\n]*"));
		rule.format = formats_.single_line_comment;
		hiliting_rules_.append(rule);
	}
}

void Hiliter::SwitchTo(const HiliteMode mode)
{
	mode_ = mode;
	hiliting_rules_.clear();
	
	switch (mode) {
	case HiliteMode::PlainText: SetupPlainText(); break;
	case HiliteMode::C_CPP: SetupC_CPP(); break;
	case HiliteMode::ShellScript: SetupShellScript(); break;
	case HiliteMode::DesktopFile: SetupDesktopFile(); break;
	default: SetupPlainText();
	}
}

}
