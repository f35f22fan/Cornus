#include "Location.hpp"

#include "../App.hpp"
#include "../io/io.hh"
#include "CompleterModel.hpp"
#include "Tab.hpp"

#include <QCompleter>
#include <functional>

namespace cornus::gui {

Location::Location(cornus::App *app): app_(app) {
	Setup();
}

Location::~Location() {
	delete completer_;
	delete model_;
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
		app_->tab()->GoTo(Action::To, {str, Processed::No});
	}
}

void Location::SetLocation(const QString &s)
{
	setText(s);
}

void Location::Setup()
{
	model_ = new CompleterModel(this);
	completer_ = new QCompleter(this);
	completer_->setModel(model_);
	setCompleter(completer_);

	completer_->setCompletionMode(QCompleter::PopupCompletion);
	completer_->setModelSorting(QCompleter::UnsortedModel);
	completer_->setFilterMode(Qt::MatchStartsWith);
	completer_->setMaxVisibleItems(5);
	completer_->setCaseSensitivity(Qt::CaseInsensitive);
	
	connect(this, &QLineEdit::returnPressed, this, &Location::HitEnter);
	connect(this, &QLineEdit::textChanged, model_,
		&CompleterModel::SetRootPath);
}

}
