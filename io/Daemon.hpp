#pragma once

#include "../category.hh"
#include "../CondMutex.hpp"
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
	CondMutex* get_exit_cm() const { return cm_; }
	io::ServerLife* life() { return life_; }
	io::Notify& notify() { return notify_; }
	const QHash<QString, Category>& possible_categories() const { return possible_categories_; }
	int signal_quit_fd() const { return signal_quit_fd_; }
	gui::TasksWin* tasks_win() const { return tasks_win_; }
	
	 // must return a copy cause it's accessed from another thread
	QProcessEnvironment env() { return env_; }
	
public Q_SLOTS:
	bool EmptyTrashRecursively(QString dir_path, const bool notify_user);
	void LoadDesktopFiles();
	void LoadDesktopFilesFrom(QString dir_path, const QProcessEnvironment &env);
	void QuitGuiApp();
	void SendAllDesktopFiles(const int fd);
	void SendDefaultDesktopFileForFullPath(ByteArray *ba, cint fd);
	void SendDesktopFilesById(cornus::ByteArray *ba, cint fd);
	void SendOpenWithList(QString mime, cint fd);
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
	
	CondMutex *cm_ = nullptr;
	gui::TasksWin *tasks_win_ = nullptr;
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	DesktopFiles desktop_files_ = {};
	io::Notify notify_ = {};
	QStringList watch_desktop_file_dirs_;
	QMimeDatabase mime_db_;
	QSystemTrayIcon *tray_icon_ = nullptr;
	QMenu *tray_menu_ = nullptr;
	io::ServerLife *life_ = nullptr;
	Category category_ = Category::None;
	QHash<QString, Category> possible_categories_;
	QProcessEnvironment env_;
	int signal_quit_fd_ = -1;
};
}
