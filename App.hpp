#pragma once

#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "gui/decl.hxx"

#include <QIcon>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMimeDatabase>
#include <QSplitter>
#include <QStackedWidget>

namespace cornus {

const QString FolderIconName = QLatin1String("special_folder");

struct DirPath {
	QString path;
	bool processed = false;
};

struct Prefs {
	bool show_hidden_files = false;
	i8 editor_tab_size = 4;
	bool show_ms_files_loaded = false;
	bool show_disk_space_free = false;
 /// -1 means not explicitly set, -2 hidden:
	i32 side_pane_width = -1;
	QMap<i8, bool> cols_visibility;
	QList<int> splitter_sizes;
};

class App : public QMainWindow {
	Q_OBJECT
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
	bool GoTo(DirPath dir_path, bool reload = false,
		QString scroll_to_and_select = QString());
	void GoUp();
	void HideTextEditor();
	void LoadIcon(io::File &file);
	gui::Location* location() { return location_; }
	QSplitter* main_splitter() const { return main_splitter_; }
	void OpenTerminal();
	Prefs& prefs() { return prefs_; }
	inline ExecInfo QueryExecInfo(io::File &file);
	ExecInfo QueryExecInfo(const QString &full_path, const QString &ext);
	void RenameSelectedFile();
	void RunExecutable(const QString &full_path, const ExecInfo &info);
	void SaveBookmarks();
	void SavePrefs();
	gui::SidePane* side_pane() const { return side_pane_; }
	gui::SidePaneItems& side_pane_items() const { return side_pane_items_; }
	gui::SidePaneModel* side_pane_model() const { return side_pane_model_; }
	void SwitchExecBitOfSelectedFiles();
	gui::Table* table() const { return table_; }
	void TellUser(const QString &msg, const QString title = QString());
	bool TestExecBuf(const char *buf, const isize size, ExecInfo &ret);
	io::Files& view_files() { return view_files_; }
	bool ViewIsAt(const QString &dir_path) const;
	
public slots:
	void GoToFinish(cornus::io::FilesData *new_data);
	
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	QMenu* CreateNewMenu();
	void CopyFileFromTo(const QString &full_path, QString to_dir);
	void CreateGui();
	void DisplayMime(io::File *file);
	void DisplaySymlinkInfo(io::File &file);
	QString GetIconName(const QString &trunc);
	QIcon* GetOrLoadIcon(const QString &icon_name);
	void GoToInitialDir();
	void GoToAndSelect(const QString full_path);
	void IconByTruncName(io::File &file, const QString &truncated, QIcon **icon = nullptr);
	void IconByFileName(io::File &file, const QString &filename, QIcon **ret_icon = nullptr);
	void LoadPrefs();
	void ProcessAndWriteTo(const QString ext,
		const QString &from_full_path, QString to_dir);
	void RegisterShortcuts();
	void SetDefaultIcon(io::File &file);
	void SetupEnvSearchPaths();
	void SetupIconNames();
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	mutable io::Files view_files_ = {};
	
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	QString theme_name_;
	QMimeDatabase mime_db_;
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
	QSplitter *main_splitter_ = nullptr;
	
	struct Notepad {
		QStackedWidget *stack = nullptr;
		gui::TextEdit *editor = nullptr;
		int window_index = -1;
		int editor_index = -1;
		QString saved_window_title;
	};
	
	Notepad notepad_ = {};
	QMenu *new_menu_ = nullptr;
	
	friend class cornus::gui::Table;
};
}
