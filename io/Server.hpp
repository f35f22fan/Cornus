#pragma once

#include "../gui/decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"

#include <QVector>
#include <QHash>

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
	void LoadDesktopFiles();
	void LoadDesktopFilesFrom(QString dir_path);
	void SendOpenWithList(QString mime, const int fd);
	void SendDesktopFilesById(cornus::ByteArray *ba, const int fd);

private:
	NO_ASSIGN_COPY_MOVE(Server);
	
	QVector<DesktopFile*> GetOrderPrefFor(QString mime);
	void SetupEnvSearchPaths();
	
	gui::TasksWin *tasks_win_ = nullptr;
	cornus::Clipboard clipboard_ = {};
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	QHash<QString, DesktopFile*> desktop_files_;
};
}
