#pragma once

#include "category.hh"
#include "cornus.hh"
#include "decl.hxx"
#include "err.hpp"
#include "io/decl.hxx"
#include "io/io.hh"
#include "io/Notify.hpp"
#include "gui/decl.hxx"
#include "TreeData.hpp"
#include "thumbnail.hh"

#include <QClipboard>

#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QMimeDatabase>
#include <QSplitter>
#include <QStackedWidget>

#include <libmtp.h>
#include <zstd.h>

QT_BEGIN_NAMESPACE
class QTabWidget;
QT_END_NAMESPACE

namespace cornus {

class App : public QMainWindow {
	Q_OBJECT
public:
	App();
	virtual ~App();
	
	void ArchiveTo(const QString &target_dir_path, const QString &ext);
	void AskCreateNewFile(io::File *file, const QString &title);
	int AvailableCpuCores() const;
	const Clipboard& clipboard() { return clipboard_; }
	QMenu* CreateNewMenu();
	i32 current_dir_id() const;
	void DeleteFilesById(const i64 id);
	Category desktop() const { return desktop_; }
	void DisplayFileContents(const int row, io::File *cloned_file = nullptr);
	void EditSelectedMovieTitle();
	void ExtractAskDestFolder();
	void ExtractTo(const QString &to_dir);
	void FileDoubleClicked(io::File *file, const gui::Column col);
	io::Files* files(const i64 files_id) const;
	QIcon* GetIcon(const QString &str);
	QIcon* GetFileIcon(io::File *file);
	QString GetPartitionFreeSpace();
	i64 GenNextFilesId();
	
	void GoUp();
	GuiBits& gui_bits() { return gui_bits_; }
	
	QColor green_color() const { return (theme_type_ == ThemeType::Light) ?
		QColor(0, 100, 0) : QColor(200, 255, 200);
	}
	
	void Reload();
	Hid* hid() const { return hid_; }
	void HideTextEditor();
	QColor hover_bg_color() const { return (theme_type_ == ThemeType::Light)
		? QColor(150, 255, 150) : QColor(0, 80, 0); }
	QColor hover_bg_color_gray(const QColor &c) const;
	void LaunchOrOpenDesktopFile(const QString &full_path, const bool has_exec_bit, const RunAction action) const;
	gui::Location* location() { return location_; }
	QSplitter* main_splitter() const { return main_splitter_; }
	Media* media() const { return media_; }
	gui::Tab *OpenNewTab(const cornus::FirstTime ft = FirstTime::No);
	void OpenTerminal();
	const QHash<QString, Category>& possible_categories() const { return possible_categories_; }
	Prefs& prefs() { return *prefs_; }
	inline ExecInfo QueryExecInfo(io::File &file);
	ExecInfo QueryExecInfo(const QString &full_path, const QString &ext);
	QString QueryMimeType(const QString &full_path);
	void RemoveAllThumbTasksExcept(const DirId dir_id);
	void RenameSelectedFile();
	void RunExecutable(const QString &full_path, const ExecInfo &info);
	void SaveBookmarks();
	void SelectCurrentTab();
	void SelectTabAt(const int tab_index);
	bool ShowInputDialog(const gui::InputDialogParams &params, QString &ret_val);
	
	void SubmitThumbLoaderBatchFromTab(QVector<ThumbLoaderArgs*> *new_work_vec, const TabId tab_id, const DirId dir_id);
	gui::Tab* tab() const; // returns current tab
	gui::Tab* tab(const TabId id, int *ret_index = nullptr);
	gui::Tab* tab_at(const int tab_index) const;
	gui::TabsWidget* tab_widget() const { return tab_widget_; }
	
	gui::TreeModel* tree_model() const { return tree_model_; }
	gui::TreeView* tree_view() const { return tree_view_; }
	TreeData& tree_data() const { return tree_data_; }
	
	void ToggleExecBitOfSelectedFiles();
	void TellUser(const QString &msg, const QString title = QString());
	void TestExecBuf(const char *buf, const isize size, ExecInfo &ret);
	ThemeType theme_type() const { return theme_type_; }
	gui::ToolBar *toolbar() const { return toolbar_; }
	
	void ViewChanged();
	HashInfo WaitForRootDaemon(const CanOverwrite co);
	
public Q_SLOTS:
	void MediaFileChanged();
	void ThumbnailArrived(cornus::Thumbnail *thumbnail);
	
protected:
	bool event(QEvent *event) override;
	void showEvent(QShowEvent *evt) override;
	
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void ArchiveAskDestArchivePath(const QString &ext);
	void ClipboardChanged(QClipboard::Mode mode);
	void CreateFilesViewPane();
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
	void InitThumbnailPoolIfNeeded();
	QIcon *LoadIcon(io::File &file);
	void LoadIconsFrom(QString dir_path);
	void OpenWithDefaultApp(const QString &full_path) const;
	int ReadMTP();
	void RegisterShortcuts();
	void RegisterVolumesListener();
	void SaveThumbnail();
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
	
	Hid *hid_ = nullptr;
	Notepad notepad_ = {};
	Clipboard clipboard_ = {};
	gui::SearchPane *search_pane_ = nullptr;
	Category desktop_ = Category::None;
	QHash<QString, Category> possible_categories_;
	ThemeType theme_type_ = ThemeType::None;
	Media *media_ = nullptr;
	gui::TabBar *tab_bar_ = nullptr;
	gui::TabsWidget *tab_widget_ = nullptr;
	cornus::GuiBits gui_bits_ = {};
	HashInfo root_hash_ = {};
	QVector<io::SaveThumbnail> thumbnails_to_save_;
	ZSTD_CCtx *compress_ctx_ = nullptr;
	
	GlobalThumbLoaderData global_thumb_loader_data_ = {};
	
	/* tabs' inotify threads keep running for a while after tabs get deleted,
	and they (the threads) need to keep accessing tab's files & mutexes which
	otherwise get deleted with the tabs, hence keep them here and
	only delete them after the corresponding thread exits */
	// ==> start
	QHash<i64, io::Files*> files_;
	i64 next_files_id_ = 0;
	// <== end
	
//	friend class cornus::gui::Table;
//	friend class cornus::gui::Tab;
};
}
