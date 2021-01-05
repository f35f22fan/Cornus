#include "Location.hpp"

#include "../App.hpp"
#include "../io/io.hh"

#include <QCompleter>
#include <QDirModel>
#include <functional>

namespace cornus::gui {

Location::Location(cornus::App *app): app_(app) {
	Setup();
}

Location::~Location() {
}

void Location::HitEnter() {
	QString str = text();
	
	if (str.isEmpty())
		return;
	
	io::FileType ft;
	if (io::FileExists(str, &ft) && ft == io::FileType::Dir) {
		app_->GoTo(str);
	}
}

void Location::SetLocation(const QString &s) {
	setText(s);
}

void Location::Setup() {
	QCompleter *completer = new QCompleter(this);
	completer->setModel(new QDirModel(completer));
	setCompleter(completer);

	completer->setCompletionMode(QCompleter::PopupCompletion);
	completer->setModelSorting(QCompleter::UnsortedModel);
	completer->setFilterMode(Qt::MatchStartsWith);
	completer->setMaxVisibleItems(5);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	
	connect(this, &QLineEdit::returnPressed, this, &Location::HitEnter);
}

}
