#include "Location.hpp"

#include "../App.hpp"
#include "../io/io.hh"

#include <QCompleter>
#include <functional>

namespace cornus::gui {

Location::Location(cornus::App *app): app_(app) {
	Setup();
}

Location::~Location() {
}

void Location::focusInEvent(QFocusEvent *evt)
{
	QLineEdit::focusInEvent(evt);
	fs_model_->setRootPath(text());
}

void Location::HitEnter()
{
	QString str = text();
	
	if (str.isEmpty())
		return;
	
	io::FileType ft;
	if (io::FileExists(str, &ft) && ft == io::FileType::Dir) {
		app_->GoTo({str, false});
	}
}

void Location::SetIncludeHiddenDirs(const bool flag)
{
	auto bits = QDir::Dirs | QDir::NoDot | QDir::NoDotDot;
	if (flag)
		bits |= QDir::Hidden;
	
	fs_model_->setFilter(bits);
}

void Location::SetLocation(const QString &s) {
	setText(s);
}

void Location::Setup() {
	QCompleter *completer = new QCompleter(this);
	fs_model_ = new QFileSystemModel(completer);
	completer->setModel(fs_model_);
	setCompleter(completer);

	completer->setCompletionMode(QCompleter::PopupCompletion);
	completer->setModelSorting(QCompleter::UnsortedModel);
	completer->setFilterMode(Qt::MatchStartsWith);
	completer->setMaxVisibleItems(5);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	
	connect(this, &QLineEdit::returnPressed, this, &Location::HitEnter);
}

}
