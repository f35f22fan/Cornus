#pragma once

#include "err.hpp"
#include "io/decl.hxx"
#include "gui/decl.hxx"

#include <QIcon>
#include <QMainWindow>
#include <QMap>
#include <QMimeDatabase>

namespace cornus {

class App : public QMainWindow {
public:
	App();
	virtual ~App();
	
	void FileDoubleClicked(io::File *file, const gui::Column col);
	void GoBack();
	void GoHome();
	bool GoTo(QString dir_path);
	void GoUp();
	void LoadIcon(io::File &file);
	void PrintMime(io::File *file);
	
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void CreateGui();
	void DisplaySymlinkInfo(io::File &file);
	QString GetIconName(const QString &trunc);
	QIcon* GetOrLoadIcon(const QString &icon_name);
	void GoToInitialDir();
	void GoToAndSelect(const QString full_path);
	void IconByTruncName(io::File &file, const QString &truncated, QIcon **icon = nullptr);
	void IconByFileName(io::File &file, const QString &filename, QIcon **ret_icon = nullptr);
	void RegisterShortcuts();
	void SetDefaultIcon(io::File &file);
	void SetupEnvSearchPaths();
	void SetupIconNames();
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	QString theme_name_;
	QMimeDatabase mime_db_;
	QIcon app_icon_;
	QString icons_dir_;
	struct IconCache {
		QIcon *folder = nullptr;
		QIcon *lib = nullptr;
		QIcon *unknown = nullptr;
	};
	IconCache icon_cache_ = {};
	QVector<QString> available_icon_names_;
	QMap<QString, QIcon*> icon_set_;
	QMap<QString, QString> icon_names_;
	QString current_dir_;
	
	gui::ToolBar *toolbar_ = nullptr;
	gui::Location *location_ = nullptr;
};
}
