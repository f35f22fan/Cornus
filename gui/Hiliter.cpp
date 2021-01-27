#include "Hiliter.hpp"

namespace cornus::gui {

Hiliter::Hiliter(QTextDocument *parent): QSyntaxHighlighter(parent)
{
	{
		formats_.keyword.setForeground(Qt::darkBlue);
		formats_.keyword.setFontWeight(QFont::Bold);
		
		formats_.klass.setFontWeight(QFont::Bold);
		formats_.klass.setForeground(Qt::darkMagenta);
		
		formats_.single_line_comment.setForeground(Qt::red);
		formats_.quotation.setForeground(Qt::darkGreen);
		
		formats_.function.setFontItalic(true);
		formats_.function.setForeground(Qt::blue);
	}
	
	multiline_comment_format_.setForeground(Qt::blue);
	comment_start_expression_ = QRegularExpression(QStringLiteral("/\\*"));
	comment_end_expression_ = QRegularExpression(QStringLiteral("\\*/"));
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
		QStringLiteral("\\bchar\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bconst\\b"),
		QStringLiteral("\\bdouble\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bexplicit\\b"),
		QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), QStringLiteral("\\bint\\b"),
		QStringLiteral("\\blong\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\boperator\\b"),
		QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"),
		QStringLiteral("\\bshort\\b"), QStringLiteral("\\bsignals\\b"), QStringLiteral("\\bsigned\\b"),
		QStringLiteral("\\bslots\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstruct\\b"),
		QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\btypename\\b"),
		QStringLiteral("\\bunion\\b"), QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bvirtual\\b"),
		QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bbool\\b")
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = formats_.keyword;
			hiliting_rules_.append(rule);
		}
	}
	
	{ /// class
		rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
		rule.format = formats_.klass;
		hiliting_rules_.append(rule);
	}
	
	{ /// single line comment
		rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
		rule.format = formats_.single_line_comment;
		hiliting_rules_.append(rule);
	}
	
	{ /// quotation
		rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
		rule.format = formats_.quotation;
		hiliting_rules_.append(rule);
	}
	
	{ /// function
		rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
		rule.format = formats_.function;
		hiliting_rules_.append(rule);
	}
}

void Hiliter::SetupPlainText() {}

void Hiliter::SetupShellScript()
{
	HighlightingRule rule;
	{ /// keywords
		const QString keywords[] = {
			QStringLiteral("\\becho\\b"), QStringLiteral("\\bread\\b"),
			QStringLiteral("\\bset\\b"), QStringLiteral("\\bunset\\b"),
			QStringLiteral("\\breadonly\\b"), QStringLiteral("\\bshift\\b"),
			QStringLiteral("\\bexport\\b"), QStringLiteral("\\bif\\b"),
			QStringLiteral("\\bfi\\b"), QStringLiteral("\\belse\\b"),
			QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"),
			QStringLiteral("\\bdone\\b"), QStringLiteral("\\bfor\\b"),
			QStringLiteral("\\buntil\\b"), QStringLiteral("\\bbreak\\b"),
			QStringLiteral("\\bcase\\b"), QStringLiteral("\\besac\\b"),
			QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\bexit\\b"),
			QStringLiteral("\\breturn\\b"), QStringLiteral("\\btrap\\b"),
			QStringLiteral("\\bwait\\b"), QStringLiteral("\\beval\\b"),
			QStringLiteral("\\bexec\\b"), QStringLiteral("\\bulimit\\b"),
			QStringLiteral("\\bumask\\b")
		};
		for (const QString &pattern : keywords) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = formats_.keyword;
			hiliting_rules_.append(rule);
		}
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
	default: SetupPlainText();
	}
}

}
