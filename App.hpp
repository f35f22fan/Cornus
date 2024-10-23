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
#include <QLocale>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QMimeDatabase>
#include <QProcessEnvironment>
#include <QShortcut>
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
	void AccessWayland();
	int app_quitting_fd() const { return app_quitting_fd_; }
	void ArchiveTo(const QString &target_dir_path, const QString &ext);
	void AskCreateNewFile(io::File *file, const QString &title);
	int AvailableCpuCores() const;
	QMenu* CreateNewMenu();
	DirId current_dir_id() const;
	void DeleteFilesById(const FilesId id);
	Category desktop() const { return desktop_; }
	void DisplayFileContents(const int row, io::File *cloned_file = nullptr);
	void EditSelectedMovieTitle();
	const QProcessEnvironment& env() const { return env_; }
	void ExtractAskDestFolder();
	void ExtractTo(const QString &to_dir);
	void FileDoubleClicked(io::File *file, const PickedBy pb);
	io::Files* files(const FilesId files_id) const;
	QIcon* GetIcon(const QString &str);
	QIcon* GetFileIcon(io::File *file);
	QString GetPartitionFreeSpace();
	FilesId GenNextFilesId();
	
	void GoUp();
	void GoTo(QStringView path);
	GuiBits& gui_bits() { return gui_bits_; }
	
	QColor green_color() const { return (theme_type_ == ThemeType::Light) ?
		QColor(0, 100, 0) : QColor(200, 255, 200);
	}
	
	Hid* hid() const { return hid_; }
	QColor hover_bg_color() const { return (theme_type_ == ThemeType::Light)
		? QColor(150, 255, 150) : QColor(0, 80, 0); }
	QColor hover_bg_color_gray(const QColor &c) const;
	void LaunchOrOpenDesktopFile(const QString &full_path, const bool has_exec_bit, const RunAction action);
	bool level_browser() const { return top_level_stack_.level == TopLevel::Browser; }
	const QLocale& locale() const { return locale_; }
	gui::Location* location() { return location_; }
	float magnify_value() const { return 2.5f; }
	float magnify_opacity() const { return 0.7f; }
	QSplitter* main_splitter() const { return main_splitter_; }
	Media* media() const { return media_; }
	gui::Tab *OpenNewTab(const cornus::FirstTime ft = FirstTime::No, QStringView full_path = QStringView());
	void OpenTerminal();
	const QHash<QString, Category>& possible_categories() const { return possible_categories_; }
	Prefs& prefs() { return *prefs_; }
	inline ExecInfo QueryExecInfo(io::File &file);
	ExecInfo QueryExecInfo(const QString &full_path, const QString &ext);
	QString QueryMimeType(const QString &full_path);
	void Reload();
	void RemoveAllThumbTasksExcept(const DirId dir_id);
	void RenameSelectedFile();
	void RunExecutable(const QString &full_path, const ExecInfo &info);
	void SaveBookmarks();
	void SelectCurrentTab();
	void SelectTabAt(const int tab_index, const FocusView fv);
	void SetTopLevel(const TopLevel tl, io::File *cloned_file = nullptr);
	bool ShowInputDialog(const gui::InputDialogParams &params, QString &ret_val);
	
	void SubmitThumbLoaderBatchFromTab(QVector<ThumbLoaderArgs*> *new_work_vec, const TabId tab_id, const DirId dir_id);
	void SubmitThumbLoaderFromTab(ThumbLoaderArgs *arg);
	gui::Tab* tab() const; // returns current tab
	gui::Tab* tab(const TabId id, int *ret_index = nullptr);
	gui::Tab* tab_at(const int tab_index) const;
	gui::TabsWidget* tab_widget() const { return tab_widget_; }
	
	gui::TreeModel* tree_model() const { return tree_model_; }
	gui::TreeView* tree_view() const { return tree_view_; }
	TreeData& tree_data() const { return tree_data_; }
	
	void ToggleExecBitOfSelectedFiles();
	void TellUser(const QString &msg, const QString title = QString());
	void TellUserDesktopFileABIDoesntMatch();
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
	void ClipboardChanged(QClipboard::Mode mode);
	
private:
	NO_ASSIGN_COPY_MOVE(App);
	
	void ApplyDefaultPrefs();
	void ArchiveAskDestArchivePath(const QString &ext);
	
	void CloseTabAt(const int i);
	void CloseCurrentTab();
	void CreateFilesViewPane();
	void CreateGui();
	void CreateSidePane();
	void DetectThemeType();
	void DisplaySymlinkInfo(io::File &file);
	QIcon* GetDefaultIcon();
	QIcon* GetFolderIcon();
	QIcon* GetIconOrLoadExisting(const QString &icon_path);
	QString GetIconThatStartsWith(const QString &trunc);
	void Init();
	void InitThumbnailPoolIfNeeded();
	QIcon *LoadIcon(io::File &file);
	void LoadIconsFrom(QString dir_path);
	void OpenWithDefaultApp(const QString &full_path);
	int ReadMTP();
	inline QShortcut* Register(const QKeySequence ks);
	void RegisterShortcuts();
	void RegisterVolumesListener();
	void SaveThumbnail();
	void SetupIconNames();
	void ShutdownThumbnailThreads();
	void TabSelected(const int i);
	
	QVector<QShortcut*> shortcuts_;
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
	
	struct TopLevelStack {
		QStackedWidget *stack = nullptr;
		gui::TextEdit *editor = nullptr;
		int window_index = -1;
		int editor_index = -1;
		int imgview_index = -1;
		TopLevel level = TopLevel::Browser;
		QString saved_window_title;
	};
	
	Hid *hid_ = nullptr;
	TopLevelStack top_level_stack_ = {};
	/*
void DropArea::paste()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (mimeData->hasImage()) {
        setPixmap(qvariant_cast<QPixmap>(mimeData->imageData()));
    } else if (mimeData->hasHtml()) {
        setText(mimeData->html());
        setTextFormat(Qt::RichText);
    } else if (mimeData->hasText()) {
        setText(mimeData->text());
        setTextFormat(Qt::PlainText);
    } else {
        setText(tr("Cannot display data"));
    }
}
	*/
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
	QProcessEnvironment env_;
	QLocale locale_;
	int app_quitting_fd_ = -1;
	
	GlobalThumbLoaderData global_thumb_loader_data_ = {};
	
	/* tabs' inotify threads keep running for a while after tabs get deleted,
	and they (the threads) need to keep accessing tab's files & mutexes which
	otherwise get deleted with the tabs, hence keep them here and
	only delete them after the corresponding thread exits */
	// ==> start
	QHash<FilesId, io::Files*> files_;
	FilesId next_files_id_ = 0;
	// <== end
	
//	friend class cornus::gui::Table;
	friend class cornus::gui::Tab;
};
}
