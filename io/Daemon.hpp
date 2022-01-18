#pragma once

#include "../category.hh"
#include "../MutexGuard.hpp"
#include "../gui/decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "io.hh"
#include "Notify.hpp"

#include <QMenu>
#include <QMimeDatabase>
#include <QProcess>
#include <QVector>
#include <QHash>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QSystemTrayIcon>

namespace cornus::io {

struct DesktopFiles {
	QHash<QString, DesktopFile*> hash;
	mutable pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	
	MutexGuard guard() const;
};

class Daemon: public QObject {
	Q_OBJECT
public:
	Daemon();
	virtual ~Daemon();
	
	DesktopFiles& desktop_files() { return desktop_files_; }
	io::ServerLife& life() { return life_; }
	io::Notify& notify() { return notify_; }
	const QHash<QString, Category>& possible_categories() const { return possible_categories_; }
	gui::TasksWin* tasks_win() const { return tasks_win_; }
	
public Q_SLOTS:
	void CutURLsToClipboard(ByteArray *ba);
	void CopyURLsToClipboard(ByteArray *ba);
	bool EmptyTrashRecursively(QString dir_path, const bool notify_user);
	void LoadDesktopFiles();
	void LoadDesktopFilesFrom(QString dir_path);
	void QuitGuiApp();
	void SendAllDesktopFiles(const int fd);
	void SendDefaultDesktopFileForFullPath(ByteArray *ba, const int fd);
	void SendDesktopFilesById(cornus::ByteArray *ba, const int fd);
	void SendOpenWithList(QString mime, const int fd);
private:
	NO_ASSIGN_COPY_MOVE(Daemon);
	
	void CheckOldThumbnails();
	void GetDesktopFilesForMime(const QString &mime,
		QVector<DesktopFile*> &show_vec, QVector<DesktopFile*> &hide_vec);
	void GetPreferredOrder(QString mime, QVector<DesktopFile *> &show_vec,
		QVector<DesktopFile *> &hide_vec);
	void InitTrayIcon();
	void SetTrayVisible(const bool yes);
	void SysTrayClicked();
	
	gui::TasksWin *tasks_win_ = nullptr;
	cornus::Clipboard clipboard_ = {};
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	DesktopFiles desktop_files_ = {};
	io::Notify notify_ = {};
	QStringList watch_desktop_file_dirs_;
	QMimeDatabase mime_db_;
	QSystemTrayIcon *tray_icon_ = nullptr;
	QMenu *tray_menu_ = nullptr;
	io::ServerLife life_ = {};
	Category category_ = Category::None;
	QHash<QString, Category> possible_categories_;
};
}
