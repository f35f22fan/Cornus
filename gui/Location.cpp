#include "Location.hpp"

#include "../App.hpp"
#include "../io/io.hh"
#include "CompleterModel.hpp"

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
}

void Location::HitEnter()
{
	QString str = text();
	
	if (str.isEmpty())
		return;
	
	io::FileType ft;
	if (io::FileExists(str, &ft) &&
		(ft == io::FileType::Dir || ft == io::FileType::Symlink))
	{
		app_->GoTo(Action::To, {str, Processed::No});
	}
}

void Location::SetLocation(const QString &s)
{
	setText(s);
}

void Location::Setup() {
	QCompleter *completer = new QCompleter(this);
	model_ = new CompleterModel(this);
	completer->setModel(model_);
	setCompleter(completer);

	completer->setCompletionMode(QCompleter::PopupCompletion);
	completer->setModelSorting(QCompleter::UnsortedModel);
	completer->setFilterMode(Qt::MatchStartsWith);
	completer->setMaxVisibleItems(5);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	
	connect(this, &QLineEdit::returnPressed, this, &Location::HitEnter);
	connect(this, &QLineEdit::textChanged, model_,
		&CompleterModel::SetRootPath);
}

}
