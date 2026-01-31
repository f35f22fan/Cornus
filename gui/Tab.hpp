#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"
#include "../io/Notify.hpp"
#include "../trash.hh"

#include <sys/fanotify.h>

#include <QList>
#include <QMenu>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QStackedWidget>

QT_BEGIN_NAMESPACE
class QScrollArea;
class QScrollBar;
QT_END_NAMESPACE

namespace cornus {

struct OpenWith {
	QString full_path;
	QString mime;
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	
	void Clear();
};

struct DirPath {
	QString path;
	Processed processed = Processed::No;
};

struct GoToParams {
	gui::Tab *tab = nullptr;
	DirPath dir_path;
	cornus::Action action = Action::None;
	bool reload = false;
	bool show_hidden_files = false;
	bool count_dir_files = false;
};

namespace gui {

class Tab: public QWidget {
	Q_OBJECT
public:
	Tab(App *app, const TabId tab_id);
	virtual ~Tab();
	
	void ActionCopy();
	void ActionCopyPaths(const Path path);
	void ActionPlayInMpv();
	void ActionCut();
	void ActionPaste();
	void ActionPasteLinks(const LinkType link);
	App* app() const { return app_; }
	void CreateGui();
	const QString& current_dir() const { return current_dir_; }
	QString CurrentDirTrashPath();
	void DeleteSelectedFiles(const ShiftPressed sp);
	void DisplayingNewDirectory(const DirId dir_id, const Reload r);
	void DragEnterEvent(QDragEnterEvent *evt);
	void DropEvent(QDropEvent *evt, const ForceDropToCurrentDir fdir);
	void ExecuteDrop(QVector<io::File *> *files_vec, io::File *to_dir,
		Qt::DropAction drop_action, Qt::DropActions possible_actions);
	QStringList FetchFilePaths(const Path path, const WhichFiles wf = WhichFiles::All);
	QList<PathAndMode> FetchFilesInfo(const WhichFiles wf = WhichFiles::All);
	TabId id() const { return id_; }
	FilesId files_id() const { return files_id_; }
	void NotivyViewsOfFileChange(const io::FileEventType evt, io::File *cloned_file = nullptr);
	int GetScrollValue() const;
	int GetVisibleFileIndex();
	void MarkLastWatchedFile();
	void MarkSelectedFilesAsWatched();
	void SetScrollValue(const int n);
	void GetSelectedArchives(QVector<QString> &urls);
	void GoBack();
	void GoForward();
	void GoHome();
	bool GoTo(const Action action, DirPath dir_path,
		const cornus::Reload r = Reload::No);
	void GoToAndSelect(const QString full_path);
	void GoToSimple(QStringView full_path);
	void GoUp();
	void GoToInitialDir();
	void FocusView();
	void HandleMouseRightClickSelection(const QPoint &pos, QSet<int> &indices);
	History* history() const { return history_; }
	gui::IconView* icon_view();
	void KeyPressEvent(QKeyEvent *evt);
	float list_speed() const { return list_speed_; }
	QString ListSpeedString() const;
	bool magnified() const { return magnified_; }
	void magnified(const bool b) { magnified_ = b; }
	io::Notify& notify() { return notify_; }
	const OpenWith& open_with() const { return open_with_; }
	void PaintMagnified(QWidget *viewport, const QStyleOptionViewItem &option);
	void PopulateUndoDelete(QMenu *menu);
	bool ReloadOpenWith();
	void ScrollToFile(const int file_index);
	void ShutdownLastInotifyThread();
	void StartDragOperation();
	
	gui::Table* table() const { return table_; }
	gui::TableModel* table_model() const { return table_model_; }
	
	void ToggleMagnifiedMode();
	
	bool ViewIsAt(const QString &dir_path) const;
	ViewMode view_mode() const { return view_mode_; }
	void SetNextView();
	void SetViewMode(const ViewMode to_mode);
	
	gui::ShiftSelect* ShiftSelect();
	void ShowRightClickMenu(const QPoint &global_pos,
		const QPoint &local_pos);
	io::Files& view_files() const;
	
	const QString& title() const { return title_; }
	void SetTitle(const QString &s);
	
	void UpdateIndices(const QSet<int> &indices);
	void UpdateView();
	
Q_SIGNALS:
	void SwitchedToNewDir(QString unprocessed_dir_path,
		QString processed_dir_path);
	
public Q_SLOTS:
	void GoToFinish(cornus::io::FilesData *new_data);
	
protected:
	virtual void resizeEvent(QResizeEvent *ev) override;
	
private:
	void AddIconsView();
	void AddOpenWithMenuTo(QMenu *main_menu, const QString &full_path);
	void AllowEfaInSelectedFiles(Efa allowed_list);
	bool AnyArchive(const QVector<QString> &extensions) const;
	QMimeData* CreateMimeWithSelectedFiles(const ClipboardAction action);
	QVector<QAction*>
	CreateOpenWithList(const QString &full_path);
	/// returns row index & cloned file if on file name, otherwise -1
	int GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file, QString *full_path = nullptr);
	void Init();
	void LaunchFromOpenWithMenu();
	void MarkFilesAsWatched(const enum Lock l, QList<io::File*> &vec);
	void ProcessAction(const QString &action);
	void ReloadThumbnail();
	void RemoveEfaFromSelectedFiles(const Efa efa, const Block block);
	void SetLastWatched(const enum Lock l, io::File *file);
	void TellEfaOfSelectedFile();
	void UndeleteFiles(const QMap<i64, QVector<cornus::trash::Names> > &items);
	QScrollBar* ViewScrollBar() const;
	int ViewRowHeight() const;
	
	App *app_ = nullptr;
	History *history_ = nullptr;
	io::Notify notify_;
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	FilesId files_id_ = -1;
	TabId id_ = -1;
	QString title_;
	QString current_dir_;
	float list_speed_ = -1.0f;
	
	QStackedWidget *viewmode_stack_ = nullptr;
	int details_view_index_ = -1, icons_view_index_ = -1;
	
	ViewMode view_mode_ = ViewMode::Details;
	gui::IconView *icon_view_ = nullptr;
	
	bool magnified_ = false;
	
	QMenu *undo_delete_menu_ = nullptr;
	OpenWith open_with_ = {};
};

}}
