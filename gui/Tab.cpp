#include "Tab.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../Prefs.hpp"
#include "../History.hpp"
#include "IconView.hpp"
#include "../io/File.hpp"
#include "Location.hpp"
#include "Table.hpp"
#include "TableModel.hpp"
#include "TreeView.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QScrollBar>

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
	//new_data->scroll_to_and_select = params->scroll_to_and_select;

	if (io::ListFiles(*new_data, &view_files) != 0) {
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
	notify_.Close();
	delete history_;
	history_ = nullptr;
	app_->DeleteFilesById(files_id_);
}

void Tab::CreateGui()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(layout);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	setContentsMargins(0, 0, 0, 0);
	
	table_model_ = new gui::TableModel(app_, this);
	table_ = new gui::Table(table_model_, app_, this);
	table_->setContentsMargins(0, 0, 0, 0);
	table_->ApplyPrefs();
	
	viewmode_stack_ = new QStackedWidget();
	details_view_index_ = viewmode_stack_->addWidget(table_);
	
	{
		QWidget *w = new QWidget();
		QBoxLayout *row = new QBoxLayout(QBoxLayout::LeftToRight);
		w->setLayout(row);
		QScrollBar *vs = new QScrollBar(Qt::Vertical);
		icon_view_ = new IconView(app_, this, vs);
		row->addWidget(icon_view_);
		row->addWidget(vs);
		icons_view_index_ = viewmode_stack_->addWidget(w);
		
		connect(vs, &QScrollBar::valueChanged, [=](int value) {
			icon_view_->update();
		});
	}
	layout->addWidget(viewmode_stack_);
}

QString Tab::CurrentDirTrashPath()
{
	QString full_path = current_dir_;
	if (!full_path.endsWith('/'))
		full_path.append('/');
	full_path.append(trash::name());
	
	return full_path;
}

void Tab::FilesChanged(const Repaint r, const int row)
{
	// File changes are monitored only in TableModel.cpp which represents
	// the detailed view, which is why the detailed view uses
	// this method to inform the icon view of changes.
	auto *p = icon_view();
	if (p != nullptr)
	{
		p->FilesChanged(r, row);
	}
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

bool Tab::GoTo(const Action action, DirPath dp, const cornus::Reload r)
{
	GoToParams *params = new GoToParams();
	params->tab = this;
	params->dir_path = dp;
	params->reload = r == Reload::Yes;
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
	}
	
	AutoDelete ad(new_data);
	int count = new_data->vec.size();
	table_->ClearMouseOver();
/// current_dir_ must be assigned before model->SwitchTo
/// otherwise won't properly show current partition's free space
	current_dir_ = new_data->processed_dir_path;
	bool got_files_to_select;
	{
		io::Files &files = *app_->files(files_id());
		got_files_to_select = !files.data.filenames_to_select.isEmpty();
	}
	table_model_->SwitchTo(new_data);
	history_->Add(new_data->action, current_dir_);
	
	if (new_data->action == Action::Back) {
		QVector<QString> list;
		history_->GetSelectedFiles(list);
		table_->SelectByLowerCase(list, NamesAreLowerCase::Yes);
	} else if (new_data->scroll_to_and_select.isEmpty()) {
		if (!got_files_to_select)
			table_->ScrollToRow(0);
	}
	
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
			QString name = cmds.select;
			if (name.startsWith('/'))
				name = io::GetFileNameOfFullPath(name).toString();
			table_model_->SelectFilenamesLater({name});
			GoTo(Action::To, {QDir::homePath(), Processed::No}, Reload::No);
			return;
		}
		if (file_type == io::FileType::Dir) {
			QString name = cmds.select;
			if (name.startsWith('/'))
				name = io::GetFileNameOfFullPath(name).toString();
			table_model_->SelectFilenamesLater({name});
			GoTo(Action::To, {cmds.go_to_path, Processed::No}, Reload::No);
		} else {
			QDir parent_dir(cmds.go_to_path);
			if (!parent_dir.cdUp()) {
				QString msg = "Can't access parent dir of file:\n\"" +
					cmds.go_to_path + QChar('\"');
				mtl_printq(msg);
				GoHome();
				return;
			}
			QString name = io::GetFileNameOfFullPath(cmds.go_to_path).toString();
			table_model_->SelectFilenamesLater({name});
			GoTo(Action::To, {parent_dir.absolutePath(), Processed::No}, Reload::No);
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
	const QString name = io::GetFileNameOfFullPath(full_path).toString();
	const SameDir same_dir = io::SameFiles(parent_dir, current_dir_) ? SameDir::Yes : SameDir::No;
	table_->model()->SelectFilenamesLater({name}, same_dir);
	
	if (same_dir == SameDir::No) {
		CHECK_TRUE_VOID(GoTo(Action::To, {parent_dir, Processed::No}, Reload::No));
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
	
	QString name = io::GetFileNameOfFullPath(current_dir_).toString();
	table_model_->SelectFilenamesLater({name});
	GoTo(Action::Up, {dir.absolutePath(), Processed::Yes}, Reload::No);
}

void Tab::GrabFocus() {
	table_->setFocus();
}

void Tab::Init()
{
	notify_.Init();
	files_id_ = app_->GenNextFilesId();
	history_ = new History(app_);
	CreateGui();
}

void Tab::PopulateUndoDelete(QMenu *menu)
{
	menu->clear();
	QMap<i64, QVector<trash::Names>> all_items;
	CHECK_TRUE_VOID(trash::ListItems(CurrentDirTrashPath(), all_items));
	
	if (all_items.isEmpty())
		return;
	
	const QString format = QLatin1String("yyyy/MM/dd hh:mm:ss");
	const int MaxMenuItemsToShow = 3;
	int counter = 0;
	int file_count = 0;
// Note: file_count != items.count(), the former is the file count,
// the latter is num items in the map.
	QMapIterator<i64, QVector<trash::Names>> i(all_items);
	i.toBack();
	while (i.hasPrevious())
	{
		i.previous();
		counter++;
		const i64 t = i.key();
		const QVector<trash::Names> &value = i.value();
		file_count += value.size();
		
		if (counter <= MaxMenuItemsToShow)
		{
			QDateTime dt = QDateTime::fromSecsSinceEpoch(t);
			QString time_str = dt.toString(format);
			QString label = time_str + QLatin1String(" (");
			label.append(QString::number(value.size()));
			label.append(')');
			QAction *action = menu->addAction(label);
			connect(action, &QAction::triggered, [=] {
				QMap<i64, QVector<trash::Names>> submap;
				submap.insert(t, value);
				UndeleteFiles(submap);
			});
			menu->addAction(action);
		}
	}
	
	if (all_items.count() > 1)
	{
		menu->addSeparator();
		QString label = tr("Undelete all files (")
			+ QString::number(file_count) + ')';
		QAction *action = menu->addAction(label);
		connect(action, &QAction::triggered, [=] {
			UndeleteFiles(all_items);
		});
		menu->addAction(action);
	}
}

void Tab::resizeEvent(QResizeEvent *ev)
{
	QWidget::resizeEvent(ev);
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

void Tab::SetViewMode(const ViewMode mode)
{
	view_mode_ = mode;
	
	switch (view_mode_) {
	case ViewMode::Details: {
		viewmode_stack_->setCurrentIndex(details_view_index_);
		break;
	}
	case ViewMode::Icons: {
		viewmode_stack_->setCurrentIndex(icons_view_index_);
		break;
	}
	default: {
		mtl_trace();
	}
	}
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

void Tab::UndeleteFiles(const QMap<i64, QVector<trash::Names>> &items)
{
	QVector<QString> filenames;
	{
		auto &files = *app_->files(files_id_);
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		for (io::File *next: vec) {
			filenames.append(next->name());
		}
	}
	
	QSet<QString> found_matches;
	{
		auto it = items.constBegin();
		while (it != items.constEnd())
		{
			const QVector<trash::Names> &vec = it.value();
			for (const trash::Names &name: vec)
			{
				if (filenames.contains(name.decoded)) {
					found_matches.insert(name.decoded);
				}
			}
			it++;
		}
	}
	
	if (!found_matches.isEmpty())
	{
		QMessageBox msg_box;
		msg_box.setText(tr("Undelete all files?"));
		QString s = tr("Warning! ") + QString::number(found_matches.count()) +
		tr(" will be overwritten!");
		msg_box.setInformativeText(s);
		msg_box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msg_box.setDefaultButton(QMessageBox::No);
		int ret = msg_box.exec();
		if (ret != QMessageBox::Yes)
			return;
	}
	
	QString current_dir = current_dir_;
	if (!current_dir.endsWith('/'))
		current_dir.append('/');
	
	const bool autorename_files = true;
	
	QString trash_dir = CurrentDirTrashPath();
	if (!trash_dir.endsWith('/'))
		trash_dir.append('/');
	
	const QString format = QLatin1String("yyyy_MMM_dd hh:mm:ss");
	QDateTime now_dt = QDateTime::fromSecsSinceEpoch(time(NULL));
	const QString renamed_suffix = tr("_AUTORENAMED_") + now_dt.toString(format);
	
	{
		QVector<QString> select_vec;
		auto it = items.constBegin();
		while (it != items.constEnd())
		{
			const QVector<trash::Names> &vec = it.value();
			for (const trash::Names &name: vec)
			{
				select_vec.append(name.decoded);
			}
			it++;
		}
		table_model_->SelectFilenamesLater(select_vec, SameDir::Yes);
	}
	
	{
		auto it = items.constBegin();
		while (it != items.constEnd())
		{
			const QVector<trash::Names> &vec = it.value();
			for (const trash::Names &name: vec)
			{
				const QString new_path_str = current_dir + name.decoded;
				const auto new_path = new_path_str.toLocal8Bit();
				
				if (autorename_files && io::FileExists(new_path.data()))
				{
					auto renamed_path = (new_path_str + renamed_suffix).toLocal8Bit();
					int status = rename(new_path.data(), renamed_path.data());
					if (status != 0) {
						mtl_status(errno);
					}
				}
				
				auto old_path = (trash_dir + name.encoded).toLocal8Bit();
				int status = rename(old_path.data(), new_path.data());
				if (status != 0) {
					mtl_status(errno);
				}
			}
			it++;
		}
	}
	
	if (io::CountDirFilesSkippingSubdirs(trash_dir) <= 0)
	{
		auto trash_ba = trash_dir.toLocal8Bit();
		int status = remove(trash_ba.data());
		if (status != 0) {
			mtl_status(errno);
		}
	}
}

io::Files&
Tab::view_files() { return *app_->files(files_id()); }

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
