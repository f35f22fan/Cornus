#pragma once

#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "gui/decl.hxx"

#include <QIcon>
#include <QMainWindow>
#include <QMap>
#include <QMimeDatabase>
#include <QSplitter>
#include <QPlainTextEdit>

namespace cornus {

const QString FolderIconName = QLatin1String("special_folder");

struct Prefs {
	bool show_hidden_files = false;
};

class App : public QMainWindow {
public:
	App();
	virtual ~App();
	
	void AskCreateNewFile(io::File *file, const QString &title);
	const QString& current_dir() const { return current_dir_; }
	void DisplayFileContents(const int row, io::File *cloned_file = nullptr);
	void FileDoubleClicked(io::File *file, const gui::Column col);
	QIcon* GetIcon(const QString &str);
	void GoBack();
	void GoHome();
	bool GoTo(QString dir_path, bool reload = false);
	void GoUp();
	void LoadIcon(io::File &file);
	void OpenTerminal();
	inline ExecInfo QueryExecInfo(io::File &file);
	ExecInfo QueryExecInfo(const QString &full_path, const QString &ext);
	void RenameSelectedFile();
	void RunExecutable(const QString &full_path, const ExecInfo &info);
	gui::SidePaneItems& side_pane_items() { return side_pane_items_; }
	gui::SidePaneModel* side_pane_model() const { return side_pane_model_; }
	void SwitchExecBitOfSelectedFiles();
	gui::Table* table() const { return table_; }
	void TellUser(const QString &msg, const QString title = QString());

private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void CreateGui();
	void DisplayMime(io::File *file);
	void DisplaySymlinkInfo(io::File &file);
	QString GetIconName(const QString &trunc);
	QIcon* GetOrLoadIcon(const QString &icon_name);
	void GoToInitialDir();
	void GoToAndSelect(const QString full_path);
	void IconByTruncName(io::File &file, const QString &truncated, QIcon **icon = nullptr);
	void IconByFileName(io::File &file, const QString &filename, QIcon **ret_icon = nullptr);
	void LoadSidePaneItems();
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
	Prefs prefs_ = {};
	
	gui::SidePane *side_pane_ = nullptr;
	gui::SidePaneModel *side_pane_model_ = nullptr;
	mutable cornus::gui::SidePaneItems side_pane_items_ = {};
//	struct SidePane {
//		gui::SidePane *pane = nullptr;
//		gui::SidePaneModel *model = nullptr;
//		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
//	};
	
	struct Notepad {
		QSplitter *splitter = nullptr;
		QPlainTextEdit *editor = nullptr;
		io::FileID last_id = {};
		QList<int> sizes = {};
	};
	
	Notepad notepad_ = {};
};
}
