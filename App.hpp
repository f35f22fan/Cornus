#pragma once

#include "category.hh"
#include "cornus.hh"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "gui/decl.hxx"
#include "TreeData.hpp"

#include <QClipboard>

#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMimeDatabase>
#include <QSplitter>
#include <QStackedWidget>

#include <libmtp.h>

QT_FORWARD_DECLARE_CLASS(QTabWidget);

namespace cornus {

class App : public QMainWindow {
	Q_OBJECT
public:
	App();
	virtual ~App();
	
	void AskCreateNewFile(io::File *file, const QString &title);
	const Clipboard& clipboard() { return clipboard_; }
	i32 current_dir_id() const;
	Category desktop() const { return desktop_; }
	void DisplayFileContents(const int row, io::File *cloned_file = nullptr);
	void EditSelectedMovieTitle();
	void ExtractAskDestFolder();
	void ExtractTo(const QString &to_dir);
	void FileDoubleClicked(io::File *file, const gui::Column col);
	QIcon* GetIcon(const QString &str);
	QIcon* GetFileIcon(io::File *file);
	QString GetPartitionFreeSpace();
	
	void GoUp();
	GuiBits& gui_bits() { return gui_bits_; }
	
	QColor green_color() const { return (theme_type_ == ThemeType::Light) ?
		QColor(0, 100, 0) : QColor(200, 255, 200);
	}
	
	void Reload();
	void HideTextEditor();
	QColor hover_bg_color() const { return (theme_type_ == ThemeType::Light)
		? QColor(150, 255, 150) : QColor(0, 80, 0); }
	QColor hover_bg_color_gray(const QColor &c) const;
	void LaunchOrOpenDesktopFile(const QString &full_path, const bool has_exec_bit, const RunAction action) const;
	gui::Location* location() { return location_; }
	QSplitter* main_splitter() const { return main_splitter_; }
	Media* media() const { return media_; }
	io::Notify& notify() { return notify_; }
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
	
	gui::Tab* tab() const; // returns current tab
	QTabWidget* tab_widget() const { return tab_widget_; }
	
	gui::TreeModel* tree_model() const { return tree_model_; }
	gui::TreeView* tree_view() const { return tree_view_; }
	TreeData& tree_data() const { return tree_data_; }
	
	void ToggleExecBitOfSelectedFiles();
	void TellUser(const QString &msg, const QString title = QString());
	void TestExecBuf(const char *buf, const isize size, ExecInfo &ret);
	ThemeType theme_type() const { return theme_type_; }
	gui::ToolBar *toolbar() const { return toolbar_; }
	
public Q_SLOTS:
	void MediaFileChanged();
	
protected:
	bool event(QEvent *event) override;
	
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void ArchiveAskDestArchivePath(const QString &ext);
	void ArchiveTo(const QString &target_dir_path, const QString &ext);
	void ClipboardChanged(QClipboard::Mode mode);
	void CreateFilesViewPane();
	QMenu* CreateNewMenu();
	void CreateGui();
	void CreateSidePane();
	void DeleteTabAt(const int i);
	void DetectThemeType();
	void DisplayMime(io::File *file);
	void DisplaySymlinkInfo(io::File &file);
	QIcon* GetDefaultIcon();
	QIcon* GetFolderIcon();
	QIcon* GetIconOrLoadExisting(const QString &icon_path);
	QString GetIconThatStartsWith(const QString &trunc);
	QIcon *LoadIcon(io::File &file);
	void LoadIconsFrom(QString dir_path);
	void OpenNewTab();
	void OpenWithDefaultApp(const QString &full_path) const;
	void ProcessAndWriteTo(const QString ext,
		const QString &from_full_path, QString to_dir);
	int ReadMTP();
	void RegisterShortcuts();
	void RegisterVolumesListener();
	void SetupIconNames();
	void TabSelected(const int i);
	
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
	
	gui::ToolBar *toolbar_ = nullptr;
	gui::Location *location_ = nullptr;
	Prefs *prefs_ = nullptr;
	
	mutable TreeData tree_data_ = {};
	gui::TreeView *tree_view_ = nullptr;
	gui::TreeModel *tree_model_ = nullptr;
	
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
	gui::SearchPane *search_pane_ = nullptr;
	Category desktop_ = Category::None;
	QHash<QString, Category> possible_categories_;
	ThemeType theme_type_ = ThemeType::None;
	Media *media_ = nullptr;
	QTabWidget *tab_widget_ = nullptr;
	cornus::GuiBits gui_bits_ = {};
	io::Notify notify_ = {};
	
	friend class cornus::gui::Table;
};
}
