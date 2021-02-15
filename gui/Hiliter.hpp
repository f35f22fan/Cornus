#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#include "decl.hxx"

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace cornus::gui {

class Hiliter : public QSyntaxHighlighter
{
	Q_OBJECT
	
public:
	Hiliter(QTextDocument *parent = 0);
	
	void SwitchTo(const HiliteMode mode);
	
protected:
	void highlightBlock(const QString &text) override;
	
private:
	
	void SetupC_CPP();
	void SetupDesktopFile();
	void SetupPlainText();
	void SetupShellScript();
	
	struct HighlightingRule
	{
		QRegularExpression pattern;
		QTextCharFormat format;
	};
	
	QVector<HighlightingRule> hiliting_rules_;
	
	QRegularExpression comment_start_expression_;
	QRegularExpression comment_end_expression_;
	
	struct Formats {
		QTextCharFormat keyword;
		QTextCharFormat klass;
		QTextCharFormat single_line_comment;
		QTextCharFormat quotation;
		QTextCharFormat function;
	} formats_;

	QTextCharFormat multiline_comment_format_;
	HiliteMode mode_ = HiliteMode::None;
};

} // cornus::gui
