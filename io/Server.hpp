#pragma once

#include "../gui/decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

#include <QObject>

namespace cornus::io {

class Server: public QObject {
	Q_OBJECT
public:
	Server();
	virtual ~Server();
	
	gui::TasksWin* tasks_win() const { return tasks_win_; }

public slots:
	void CutToClipboard(const QString &s);
	void CopyToClipboard(const QString &s);

private:
	NO_ASSIGN_COPY_MOVE(Server);
	
	gui::TasksWin *tasks_win_ = nullptr;
	cornus::Clipboard clipboard_ = {};
};
}
