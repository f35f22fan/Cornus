#pragma once

#include "category.hh"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "gui/decl.hxx"
#include "SidePaneItems.hpp"

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
	QIcon* GetFileIcon(io::File *file);
	QString GetPartitionFreeSpace();
	void GoBack();
	void GoForward();
	void GoHome();
	bool GoTo(const Action action, DirPath dir_path, const cornus::Reload r = Reload::No,
		QString scroll_to_and_select = QString());
	QColor green_color() const { return (theme_type_ == ThemeType::Light) ?
		QColor(0, 100, 0) : QColor(200, 255, 200);
	}
	void Reload();
	void GoUp();
	void HideTextEditor();
	QColor hover_bg_color() const { return (theme_type_ == ThemeType::Light)
		? QColor(150, 255, 150) : QColor(0, 80, 0); }
	QColor hover_bg_color_gray(const QColor &c) const;
	void LaunchOrOpenDesktopFile(const QString &full_path, const bool has_exec_bit, const RunAction action) const;
	gui::Location* location() { return location_; }
	QSplitter* main_splitter() const { return main_splitter_; }
	Media* media() const { return media_; }
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
	SidePaneItems& side_pane_items() const { return side_pane_items_; }
	gui::SidePaneModel* side_pane_model() const { return side_pane_model_; }
	void SwitchExecBitOfSelectedFiles();
	gui::Table* table() const { return table_; }
	gui::TableModel* table_model() const { return table_model_; }
	void TellUser(const QString &msg, const QString title = QString());
	void TestExecBuf(const char *buf, const isize size, ExecInfo &ret);
	ThemeType theme_type() const { return theme_type_; }
	gui::ToolBar *toolbar() const { return toolbar_; }
	io::Files& view_files() { return view_files_; }
	bool ViewIsAt(const QString &dir_path) const;
	
public slots:
	void GoToFinish(cornus::io::FilesData *new_data);
	void MediaFileChanged();
	
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
	QIcon* GetDefaultIcon();
	QIcon* GetFolderIcon();
	QIcon* GetIconOrLoadExisting(const QString &icon_path);
	QString GetIconThatStartsWith(const QString &trunc);
	void GoToInitialDir();
	void GoToAndSelect(const QString full_path);
	QIcon *LoadIcon(io::File &file);
	void LoadIconsFrom(QString dir_path);
	void OpenWithDefaultApp(const QString &full_path) const;
	void ProcessAndWriteTo(const QString ext,
		const QString &from_full_path, QString to_dir);
	void RegisterShortcuts();
	void SetupIconNames();
	void ShutdownLastInotifyThread();
	void UdisksFunc();
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	mutable io::Files view_files_ = {};
	
	QVector<QString> search_icons_dirs_;
	QVector<QString> xdg_data_dirs_;
	QString theme_name_;
	QMimeDatabase mime_db_;
	struct IconCache {
		QIcon *folder = nullptr;
		QIcon *lib = nullptr;
		QIcon *unknown = nullptr;
	} icon_cache_ = {};
	QHash<QString, QIcon*> icon_set_;
	QHash<QString, QString> icon_names_;
	QString current_dir_;
	
	gui::ToolBar *toolbar_ = nullptr;
	gui::Location *location_ = nullptr;
	Prefs *prefs_ = nullptr;
	
	gui::SidePane *side_pane_ = nullptr;
	gui::SidePaneModel *side_pane_model_ = nullptr;
	mutable SidePaneItems side_pane_items_ = {};
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
	QString title_;
	Media *media_ = nullptr;
	
	friend class cornus::gui::Table;
};
}
