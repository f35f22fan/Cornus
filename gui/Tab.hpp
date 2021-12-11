#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"
#include "../io/Notify.hpp"
#include "../trash.hh"

#include <QMenu>
#include <QTabBar>
#include <QStackedWidget>

QT_FORWARD_DECLARE_CLASS(QScrollArea)

namespace cornus {

struct DirPath {
	QString path;
	Processed processed = Processed::No;
};

struct GoToParams {
	gui::Tab *tab = nullptr;
	DirPath dir_path;
	cornus::Action action = Action::None;
	bool reload;
};

namespace gui {

class Tab: public QWidget {
	Q_OBJECT
public:
	Tab(App *app);
	virtual ~Tab();
	
	App* app() const { return app_; }
	void CreateGui();
	const QString& current_dir() const { return current_dir_; }
	QString CurrentDirTrashPath();
	i64 files_id() const { return files_id_; }
	
	void GoBack();
	void GoForward();
	void GoHome();
	bool GoTo(const Action action, DirPath dir_path, const cornus::Reload r = Reload::No);
	void GoToAndSelect(const QString full_path);
	void GoToSimple(const QString &full_path);
	void GoUp();
	void GoToInitialDir();
	
	void GrabFocus();
	History* history() const { return history_; }
	gui::IconView* icon_view() const { return icon_view_; }
	io::Notify& notify() { return notify_; }
	
	void PopulateUndoDelete(QMenu *menu);
	
	void ShutdownLastInotifyThread();
	
	gui::Table* table() const { return table_; }
	gui::TableModel* table_model() const { return table_model_; }
	
	bool ViewIsAt(const QString &dir_path) const;
	ViewMode view_mode() const { return view_mode_; }
	void SetViewMode(const ViewMode mode);
	
	const QString& title() const { return title_; }
	void SetTitle(const QString &s);
	
public Q_SLOTS:
	void GoToFinish(cornus::io::FilesData *new_data);
	
private:
	void Init();
	void UndeleteFiles(const QMap<i64, QVector<cornus::trash::Names> > &items);
	
	App *app_ = nullptr;
	History *history_ = nullptr;
	io::Notify notify_;
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	i64 files_id_ = -1;
	QString title_;
	QString current_dir_;
	
	QStackedWidget *stack_ = nullptr;
	int details_view_index_ = -1, icons_view_index_ = -1;
	
	ViewMode view_mode_ = ViewMode::Details;
	gui::IconView *icon_view_ = nullptr;
	QScrollArea *icon_scroll_area_ = nullptr;
};

}}
