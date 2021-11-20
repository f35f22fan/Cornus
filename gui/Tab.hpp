#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"
#include "../io/io.hh"

#include <QTabBar>

namespace cornus {

struct DirPath {
	QString path;
	Processed processed = Processed::No;
};

struct GoToParams {
	gui::Tab *tab = nullptr;
	DirPath dir_path;
	QString scroll_to_and_select;
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
	
	void GoBack();
	void GoForward();
	void GoHome();
	bool GoTo(const Action action, DirPath dir_path, const cornus::Reload r = Reload::No,
		QString scroll_to_and_select = QString());
	void GoToAndSelect(const QString full_path);
	void GoToSimple(const QString &full_path);
	void GoUp();
	void GoToInitialDir();
	
	void GrabFocus();
	History* history() const { return history_; }
	void ShutdownLastInotifyThread();
	
	gui::Table* table() const { return table_; }
	gui::TableModel* table_model() const { return table_model_; }
	io::Files& view_files() { return view_files_; }
	
	bool ViewIsAt(const QString &dir_path) const;
	
	const QString& title() const { return title_; }
	void SetTitle(const QString &s);
	
public Q_SLOTS:
	void GoToFinish(cornus::io::FilesData *new_data);
	
private:
	void Init();
	
	App *app_ = nullptr;
	History *history_ = nullptr;
	
	gui::Table *table_ = nullptr; // owned by QMainWindow
	gui::TableModel *table_model_ = nullptr; // owned by table_
	mutable io::Files view_files_ = {};
	QString title_;
	QString current_dir_;
};

}}
