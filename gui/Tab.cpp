#include "Tab.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../Prefs.hpp"
#include "../History.hpp"
#include "Location.hpp"
#include "Table.hpp"
#include "TableModel.hpp"
#include "TreeView.hpp"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>

namespace cornus {

void FigureOutSelectPath(QString &select_path, QString &go_to_path)
{
	if (select_path.isEmpty())
		return;
	
	if (go_to_path.isEmpty()) {
/// @go_to_path is empty
/// @select_path is absolute path
		if (!io::FileExists(select_path)) {
			QString msg = "File to select doesn't exist:\n\""
				+ select_path + QChar('\"');
			mtl_printq(msg);
			select_path.clear();
			return;
		}
		
		QDir parent_dir(select_path);
		if (!parent_dir.cdUp()) {
			mtl_warn("Can't access parent dir of file to select");
			go_to_path = QDir::homePath();
			select_path.clear();
			return;
		}
		go_to_path = parent_dir.absolutePath();
		return;
	}
	
/// @go_to_path is NOT Empty
	if (select_path.startsWith('/')) {
		return;
	}
	
	if (!go_to_path.endsWith('/'))
		go_to_path.append('/');
	
	QString test_path = go_to_path + select_path;
	
	if (io::FileExists(test_path)) {
		select_path = test_path;
		return;
	}
	
	if (!io::FileExists(go_to_path))
		go_to_path = QDir::homePath();
	select_path.clear();
}

void* GoToTh(void *p)
{
	pthread_detach(pthread_self());
	GoToParams *params = (GoToParams*)p;
	gui::Tab *tab = params->tab;
	App *app = tab->app();

	if (!params->reload) {
		if (tab->ViewIsAt(params->dir_path.path)) {
			delete params;
			return nullptr;
		}
	}

	if (!params->dir_path.path.endsWith('/'))
		params->dir_path.path.append('/');
	
	io::FilesData *new_data = new io::FilesData();
	io::Files &view_files = *app->files(tab->files_id());
	{
		MutexGuard guard = view_files.guard();
		new_data->sorting_order = view_files.data.sorting_order;
	}

	new_data->action = params->action;
	new_data->start_time = std::chrono::steady_clock::now();
	new_data->show_hidden_files(app->prefs().show_hidden_files());
	if (params->dir_path.processed == Processed::Yes)
		new_data->processed_dir_path = params->dir_path.path;
	else
		new_data->unprocessed_dir_path = params->dir_path.path;
	new_data->scroll_to_and_select = params->scroll_to_and_select;

	if (io::ListFiles(*new_data, &view_files) != io::Err::Ok) {
		delete params;
		delete new_data;
		return nullptr;
	}
///#define CORNUS_WAITED_FOR_WIDGETS
	GuiBits &gui_bits = app->gui_bits();
	gui_bits.Lock();
	while (!gui_bits.created())
	{
		using Clock = std::chrono::steady_clock;
		new_data->start_time = std::chrono::time_point<Clock>::max();
#ifdef CORNUS_WAITED_FOR_WIDGETS
		auto start_time = std::chrono::steady_clock::now();
#endif
		const int status = gui_bits.CondWait();
		if (status != 0) {
			mtl_status(status);
			break;
		}
#ifdef CORNUS_WAITED_FOR_WIDGETS
		auto now = std::chrono::steady_clock::now();
		const float elapsed = std::chrono::duration<float,
			std::chrono::milliseconds::period>(now - start_time).count();
		
		mtl_info("Waited for gui creation: %.1f ms", elapsed);
#endif
	}
	gui_bits.Unlock();
	QMetaObject::invokeMethod(tab, "GoToFinish",
		Q_ARG(cornus::io::FilesData*, new_data));
	delete params;
	return nullptr;
}

namespace gui {

Tab::Tab(App *app) : app_(app)
{
	Init();
}

Tab::~Tab()
{
	/// tab must be deleted before prefs_ because table_model_ calls 
	/// into prefs().show_free_partition_space() in TableModel::GetName()
	ShutdownLastInotifyThread();
	delete table_;
	delete history_;
	history_ = nullptr;
	app_->DeleteFilesById(files_id_);
}

void Tab::CreateGui()
{
	setContentsMargins(0, 0, 0, 0);
	
	table_model_ = new gui::TableModel(app_, this);
	table_ = new gui::Table(table_model_, app_, this);
	table_->setContentsMargins(0, 0, 0, 0);
	table_->ApplyPrefs();
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(layout);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(table_);
}

void Tab::GoBack() {
	QString s = history_->Back();
	if (s.isEmpty())
		return;
	
	GoTo(Action::Back, {s, Processed::Yes});
}

void Tab::GoForward() {
	QString s = history_->Forward();
	if (s.isEmpty())
		return;
	
	GoTo(Action::Forward, {s, Processed::Yes});
}

void Tab::GoHome() { GoTo(Action::To, {QDir::homePath(), Processed::No}); }

bool Tab::GoTo(const Action action, DirPath dp, const cornus::Reload r, QString scroll_to_and_select)
{
	GoToParams *params = new GoToParams();
	params->tab = this;
	params->dir_path = dp;
	params->reload = r == Reload::Yes;
	params->scroll_to_and_select = scroll_to_and_select;
	params->action = action;
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::GoToTh, params);
	if (status != 0)
		mtl_warn("pthread_create: %s", strerror(errno));
	
	return true;
}

void Tab::GoToFinish(cornus::io::FilesData *new_data)
{
	if (new_data->action != Action::Back)
	{
		history_->Record();
		for (auto &next: history_->recorded()) {
			mtl_printq2("recorded: ", next);
		}
	}
	
	AutoDelete ad(new_data);
	int count = new_data->vec.size();
	table_->ClearMouseOver();
/// current_dir_ must be assigned before model->SwitchTo
/// otherwise won't properly show current partition's free space
	current_dir_ = new_data->processed_dir_path;
	table_model_->SwitchTo(new_data);
	history_->Add(new_data->action, current_dir_);
	
	if (new_data->action == Action::Back) {
		QVector<QString> list;
		history_->GetSelectedFiles(list);
		table_->SelectByLowerCase(list);
	} else if (new_data->scroll_to_and_select.isEmpty())
		table_->ScrollToRow(0);
	
	QString dir_name = QDir(new_data->processed_dir_path).dirName();
	if (dir_name.isEmpty())
		dir_name = new_data->processed_dir_path; // likely "/"
	
	using Clock = std::chrono::steady_clock;
	if (app_->prefs().show_ms_files_loaded() &&
		(new_data->start_time != std::chrono::time_point<Clock>::max()))
	{
		auto now = std::chrono::steady_clock::now();
		const float elapsed = std::chrono::duration<float,
			std::chrono::milliseconds::period>(now - new_data->start_time).count();
		QString diff_str = io::FloatToString(elapsed, 2);
		
		QString s = dir_name + QLatin1String(" (") + QString::number(count)
			+ QChar('/') + diff_str + QLatin1String(" ms)");
		SetTitle(s);
	} else {
		SetTitle(dir_name);
	}
	app_->SelectCurrentTab();
}

void Tab::GoToInitialDir()
{
	const QStringList args = QCoreApplication::arguments();
	
	if (args.size() <= 1) {
		GoHome();
		return;
	}
	
	struct Commands {
		QString select;
		QString go_to_path;
	} cmds = {};
	
	const QString SelectCmdName = QLatin1String("select");
	const QString CmdPrefix = QLatin1String("--");
	const int arg_count = args.size();
	
	QString *next_command = nullptr;
	for (int i = 1; i < arg_count; i++)
	{
		const QString &next = args[i];
		
		if (next_command != nullptr) {
			*next_command = next;
			next_command = nullptr;
			continue;
		}
		
		if (next.startsWith(CmdPrefix)) {
			if (next.endsWith(SelectCmdName)) {
				next_command = &cmds.select;
				continue;
			}
		} else {
			if (next.startsWith(QLatin1String("file://"))) {
				cmds.go_to_path = QUrl(next).toLocalFile();
			} else if (!next.startsWith('/')) {
				cmds.go_to_path = QCoreApplication::applicationDirPath()
					+ '/' + next;
			} else {
				cmds.go_to_path = next;
			}
		}
	}

	FigureOutSelectPath(cmds.select, cmds.go_to_path);
	if (!cmds.go_to_path.isEmpty())
	{
		io::FileType file_type;
		if (!io::FileExists(cmds.go_to_path, &file_type)) {
			QString msg = "Directory doesn't exist:\n\""
				+ cmds.go_to_path + QChar('\"');
			mtl_printq(msg);
			GoTo(Action::To, {QDir::homePath(), Processed::No}, Reload::No, cmds.select);
			return;
		}
		if (file_type == io::FileType::Dir) {
			GoTo(Action::To, {cmds.go_to_path, Processed::No}, Reload::No, cmds.select);
		} else {
			QDir parent_dir(cmds.go_to_path);
			if (!parent_dir.cdUp()) {
				QString msg = "Can't access parent dir of file:\n\"" +
					cmds.go_to_path + QChar('\"');
				mtl_printq(msg);
				GoHome();
				return;
			}
			GoTo(Action::To, {parent_dir.absolutePath(), Processed::No}, Reload::No, cmds.go_to_path);
			return;
		}
	}
}

void Tab::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	CHECK_TRUE_VOID(parent.exists());
	QString parent_dir = parent.absolutePath();
	
	if (!io::SameFiles(parent_dir, current_dir_)) {
		CHECK_TRUE_VOID(GoTo(Action::To, {parent_dir, Processed::No}, Reload::No, full_path));
	} else {
		table_->ScrollToAndSelect(full_path);
	}
}

void Tab::GoToSimple(const QString &full_path) {
	GoTo(Action::To, {full_path, Processed::No});
}

void Tab::GoUp()
{
	if (current_dir_.isEmpty())
		return;
	
	QDir dir(current_dir_);
	
	if (!dir.cdUp())
		return;
	
	QString select_path = current_dir_;
	GoTo(Action::Up, {dir.absolutePath(), Processed::Yes}, Reload::No, select_path);
}

void Tab::GrabFocus() {
	table_->setFocus();
}

void Tab::Init()
{
	files_id_ = app_->GenNextFilesId();
	history_ = new History(app_);
	CreateGui();
}

void Tab::SetTitle(const QString &s)
{
	title_ = s;
	QTabWidget *w = app_->tab_widget();
	int index = w->indexOf(this);
	QString short_title = title_;
	if (short_title.size() > 10) {
		short_title = short_title.mid(0, 10);
		short_title += QLatin1String("..");
	}
	
	w->setTabText(index, short_title);
}

void Tab::ShutdownLastInotifyThread()
{
	io::Files *p = app_->files(files_id_);
	CHECK_PTR_VOID(p);
	auto &files = *p;
	files.Lock();
#ifdef CORNUS_WAITED_FOR_WIDGETS
	auto start_time = std::chrono::steady_clock::now();
#endif
	files.data.thread_must_exit(true);
	
	while (!files.data.thread_exited())
	{
		int status = files.CondWait();
		if (status != 0) {
			mtl_status(status);
			break;
		}
	}
	
	files.Unlock();
#ifdef CORNUS_WAITED_FOR_WIDGETS
	auto now = std::chrono::steady_clock::now();
	const float elapsed = std::chrono::duration<float,
		std::chrono::milliseconds::period>(now - start_time).count();
	
	mtl_info("Waited for thread termination: %.1f ms", elapsed);
#endif
}

bool Tab::ViewIsAt(const QString &dir_path) const
{
	QString old_dir_path;
	{
		auto &files = *app_->files(files_id_);
		MutexGuard guard = files.guard();
		old_dir_path = files.data.processed_dir_path;
	}
	
	if (old_dir_path.isEmpty()) {
		return false;
	}
	
	return io::SameFiles(dir_path, old_dir_path);
}

}} // cornus::gui
