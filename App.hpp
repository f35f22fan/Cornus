#pragma once

#include "category.hh"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "gui/decl.hxx"

#include <QClipboard>
#include <QHash>
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
	Processed processed = Processed::No;
};

class App : public QMainWindow {
	Q_OBJECT
public:
	App();
	virtual ~App();
	
	void AskCreateNewFile(io::File *file, const QString &title);
	const Clipboard& clipboard() { return clipboard_; }
	const QString& current_dir() const { return current_dir_; }
	i32 current_dir_id() const;
	Category desktop() const { return desktop_; }
	void DisplayFileContents(const int row, io::File *cloned_file = nullptr);
	void ExtractAskDestFolder();
	void ExtractTo(const QString &to_dir);
	void FileDoubleClicked(io::File *file, const gui::Column col);
	QIcon* GetIcon(const QString &str);
	void GoBack();
	void GoHome();
	bool GoTo(const Action action, DirPath dir_path, const cornus::Reload r = Reload::No,
		QString scroll_to_and_select = QString());
	void Reload();
	void GoUp();
	void HideTextEditor();
	void LaunchOrOpenDesktopFile(const QString &full_path, const bool has_exec_bit, const RunAction action) const;
	void LoadIcon(io::File &file);
	gui::Location* location() { return location_; }
	QSplitter* main_splitter() const { return main_splitter_; }
	void OpenTerminal();
	const QHash<QString, Category>& possible_categories() const { return possible_categories_; }
	Prefs& prefs() { return *prefs_; }
	inline ExecInfo QueryExecInfo(io::File &file);
	ExecInfo QueryExecInfo(const QString &full_path, const QString &ext);
	QString QueryMimeType(const QString &full_path);
	void RenameSelectedFile();
	void RunExecutable(const QString &full_path, const ExecInfo &info);
	void SaveBookmarks();
	bool ShowInputDialog(const gui::InputDialogParams &params, QString &ret_val);
	gui::SidePane* side_pane() const { return side_pane_; }
	gui::SidePaneItems& side_pane_items() const { return side_pane_items_; }
	gui::SidePaneModel* side_pane_model() const { return side_pane_model_; }
	void SwitchExecBitOfSelectedFiles();
	gui::Table* table() const { return table_; }
	void TellUser(const QString &msg, const QString title = QString());
	bool TestExecBuf(const char *buf, const isize size, ExecInfo &ret);
	ThemeType theme_type() const { return theme_type_; }
	io::Files& view_files() { return view_files_; }
	bool ViewIsAt(const QString &dir_path) const;
	
public slots:
	void GoToFinish(cornus::io::FilesData *new_data);
	
protected:
	bool event(QEvent *event) override;
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void ArchiveAskDestArchivePath(const QString &ext);
	void ArchiveTo(const QString &target_dir_path, const QString &ext);
	void ClipboardChanged(QClipboard::Mode mode);
	QMenu* CreateNewMenu();
	void CreateGui();
	void DetectThemeType();
	void DisplayMime(io::File *file);
	void DisplaySymlinkInfo(io::File &file);
	QString GetIconName(const QString &trunc);
	QIcon* GetOrLoadIcon(const QString &icon_name);
	void GoToInitialDir();
	void GoToAndSelect(const QString full_path);
	void IconByTruncName(io::File &file, const QString &truncated, QIcon **icon = nullptr);
	void IconByFileName(io::File &file, const QString &filename, QIcon **ret_icon = nullptr);
	void OpenWithDefaultApp(const QString &full_path) const;
	void ProcessAndWriteTo(const QString ext,
		const QString &from_full_path, QString to_dir);
	void RegisterShortcuts();
	void SetDefaultIcon(io::File &file);
	void SetupIconNames();
	void ShutdownLastInotifyThread();
	
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
	Prefs *prefs_ = nullptr;
	
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
	Clipboard clipboard_ = {};
	History *history_ = nullptr;
	gui::SearchPane *search_pane_ = nullptr;
	Category desktop_ = Category::None;
	QHash<QString, Category> possible_categories_;
	ThemeType theme_type_ = ThemeType::None;
	
	friend class cornus::gui::Table;
};
}
