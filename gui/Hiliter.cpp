#include "Hiliter.hpp"

namespace cornus::gui {

Hiliter::Hiliter(QTextDocument *parent): QSyntaxHighlighter(parent)
{
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

void Hiliter::SetupPlainText() {
	hiliting_rules_.clear();
}

void Hiliter::SetupC_CPP()
{
	hiliting_rules_.clear();
	HighlightingRule rule;
	QTextCharFormat format;
	
	{ /// keywords
		format = {};
		format.setForeground(Qt::darkBlue);
		format.setFontWeight(QFont::Bold);
		
		const QString keywordPatterns[] = {
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
		for (const QString &pattern : keywordPatterns) {
			rule.pattern = QRegularExpression(pattern);
			rule.format = format;
			hiliting_rules_.append(rule);
		}
	}
	
	{ /// class
		format = {};
		format.setFontWeight(QFont::Bold);
		format.setForeground(Qt::darkMagenta);
		rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
		rule.format = format;
		hiliting_rules_.append(rule);
	}
	
	{ /// single line comment
		format = {};
		format.setForeground(Qt::red);
		rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
		rule.format = format;
		hiliting_rules_.append(rule);
	}
	
	{ /// quotation
		format = {};
		format.setForeground(Qt::darkGreen);
		rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
		rule.format = format;
		hiliting_rules_.append(rule);
	}
	
	{ /// function
		format = {};
		format.setFontItalic(true);
		format.setForeground(Qt::blue);
		rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
		rule.format = format;
		hiliting_rules_.append(rule);
	}
}

void Hiliter::SwitchTo(const HiliteMode mode)
{
	switch (mode) {
	case HiliteMode::PlainText: SetupPlainText(); break;
	case HiliteMode::C_CPP: SetupC_CPP(); break;
	default: SetupPlainText();
	}
}

}
