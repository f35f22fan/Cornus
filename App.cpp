// This has to be the first include otherwise gdbusintrospection.h causes an error.
extern "C" {
#include <dconf.h>
};
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>

#include "App.hpp"
#include "AutoDelete.hh"
#include "io/disks.hh"
#include "io/io.hh"
#include "io/File.hpp"
#include "io/socket.hh"
#include "gui/Location.hpp"
#include "gui/SidePane.hpp"
#include "gui/SidePaneItem.hpp"
#include "gui/SidePaneModel.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "gui/TextEdit.hpp"
#include "gui/ToolBar.hpp"
#include "prefs.hh"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QUrl>

namespace cornus {

void* AutoLoadServerIfNeeded(void *arg)
{
	pthread_detach(pthread_self());
	ByteArray ba;
	ba.add_msg_type(io::socket::MsgBits::CheckAlive);
	
	if (io::socket::SendSync(ba)) {
		mtl_info("Server is online");
		return nullptr;
	}
	
	QString excl_file_path = QDir::homePath() + QLatin1String("/.cornus_excl_");
	auto excl_ba = excl_file_path.toLocal8Bit();
	int fd = open(excl_ba.data(), O_EXCL | O_CREAT, 0x777);
	
	if (fd == -1) {
		mtl_info("someone is already trying to start the io server");
		return nullptr; // someone is already trying to start the io server
	}
	
	mtl_info("Starting io server..");
	QString server_dir_path = QCoreApplication::applicationDirPath();
	QString server_full_path = server_dir_path + QLatin1String("/cornus_io");
	
	QStringList arguments;
	QProcess::startDetached(server_full_path, arguments, server_dir_path);
	mtl_info("done");
	
	sleep(5);
	int status = remove(excl_ba.data());
	if (status != 0) {
		mtl_status(errno);
	}
	
	///App *app = (App*)arg;
	return nullptr;
}

struct GoToParams {
	App *app = nullptr;
	DirPath dir_path;
	QString scroll_to_and_select;
	bool reload;
};

void* GoToTh(void *p)
{
	pthread_detach(pthread_self());

	GoToParams *params = (GoToParams*)p;
	App *app = params->app;

	if (!params->reload) {
		if (app->ViewIsAt(params->dir_path.path)) {
			delete params;
			return nullptr;
		}
	}

	if (!params->dir_path.path.endsWith('/'))
		params->dir_path.path.append('/');
	
	io::FilesData *new_data = new io::FilesData();
	io::Files &view_files = app->view_files();
	{
		MutexGuard guard(&view_files.mutex);
		new_data->sorting_order = view_files.data.sorting_order;
	}

	new_data->start_time = std::chrono::steady_clock::now();
	new_data->show_hidden_files = app->prefs().show_hidden_files;
	if (params->dir_path.processed)
		new_data->processed_dir_path = params->dir_path.path;
	else
		new_data->unprocessed_dir_path = params->dir_path.path;
	new_data->scroll_to_and_select = params->scroll_to_and_select;

	if (io::ListFiles(*new_data, &view_files) != io::Err::Ok) {
		delete params;
		delete new_data;
		return nullptr;
	}

	view_files.Lock();
	while (!view_files.data.widgets_created)
	{
		using Clock = std::chrono::steady_clock;
		new_data->start_time = std::chrono::time_point<Clock>::max();
		auto start_time = std::chrono::steady_clock::now();
		int status = pthread_cond_wait(&view_files.cond, &view_files.mutex);
		if (status != 0) {
			mtl_status(status);
			break;
		}
		auto now = std::chrono::steady_clock::now();
		const float elapsed = std::chrono::duration<float,
			std::chrono::milliseconds::period>(now - start_time).count();
		//mtl_info("Waited: %.5f ms", elapsed);
	}
	view_files.Unlock();
	
	QMetaObject::invokeMethod(app, "GoToFinish",
		Q_ARG(cornus::io::FilesData*, new_data));

	delete params;
	return nullptr;
}

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

App::App()
{
	qRegisterMetaType<cornus::io::FilesData*>();
	qRegisterMetaType<cornus::PartitionEvent*>();
	qDBusRegisterMetaType<QMap<QString, QVariant>>();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, gui::sidepane::LoadItems, this);
	if (status != 0)
		mtl_status(status);
	
	status = pthread_create(&th, NULL, AutoLoadServerIfNeeded, this);
	if (status != 0)
		mtl_status(status);
	
	setWindowIcon(QIcon(cornus::AppIconPath));
	GoToInitialDir();
	SetupIconNames();
	SetupEnvSearchPaths();
	CreateGui();
	RegisterShortcuts();
	resize(900, 700);
	table_->setFocus();
}

App::~App() {
	QMapIterator<QString, QIcon*> i(icon_set_);
	while (i.hasNext()) {
		i.next();
		QIcon *icon = i.value();
		delete icon;
	}
	
	MutexGuard guard(&side_pane_items_.mutex);
	for (auto *item: side_pane_items_.vec)
		delete item;
	
	side_pane_items_.vec.clear();
}

void
App::AskCreateNewFile(io::File *file, const QString &title)
{
	AutoDelete ad(file);
	const QString text = file->name();
	
	QDialog dialog(this);
	dialog.setWindowTitle(title);
	dialog.setModal(true);
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	dialog.setLayout(vert_layout);
	
	{ // icon + mime row
		QBoxLayout *row = new QBoxLayout(QBoxLayout::LeftToRight);
		
		LoadIcon(*file);
		QLabel *icon_label = new QLabel();
		const QIcon *icon = file->cache().icon;
		QPixmap pixmap = icon->pixmap(QSize(64, 64));
		icon_label->setPixmap(pixmap);
		row->addWidget(icon_label);

		QLabel *text_label = new QLabel();
		text_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		text_label->setText("<b>" + tr("Name:") + "</b>");
		row->addWidget(text_label, 2);
		
		vert_layout->addLayout(row);
	}
	
	QLineEdit *le = new QLineEdit();
	{ // file name row
		vert_layout->addWidget(le);
		le->setText(text);
		
		const QString tar = QLatin1String(".tar.");
		int index = text.lastIndexOf(tar);
		if (index > 0) {
			le->setSelection(0, index);
		} else {
			index = text.lastIndexOf('.');
			if (index > 0 && index < (text.size() - 1)) {
				le->setSelection(0, index);
			} else {
				le->setSelection(0, text.size());
			}
		}
	}
	
	{ // ok + cancel buttons row
		QDialogButtonBox *button_box = new QDialogButtonBox
			(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		vert_layout->addWidget(button_box);
	}
	
	QFontMetrics fm = table_->fontMetrics();
	int w = fm.boundingRect(text).width();
	w = std::min(800, std::max(w + 80, 400)); // between 300 and 800
	dialog.resize(w, 100);
	bool ok = dialog.exec();
	if (!ok)
		return;
	
	QString value = le->text().trimmed();
	if (value.isEmpty())
		return;
	
	file->name(value);
	auto path_ba = file->build_full_path().toLocal8Bit();
	int fd;
	
	if (file->is_dir()) {
		fd = mkdir(path_ba.data(), 0775);
	} else {
		fd = open(path_ba.data(), O_RDWR | O_CREAT | O_EXCL, 0664);
	}
	
	if (fd == -1) {
		if (errno == EEXIST) {
			QString name = QString("\"") + file->name();
			TellUser(name + "\" already exists", tr("Failed"));
		} else {
			QString msg = QString("Couldn't create file: ") +
				strerror(errno);
			TellUser(msg, tr("Failed"));
		}
	} else {
		::close(fd);
	}
}

void App::CreateGui()
{
	toolbar_ = new gui::ToolBar(this);
	location_ = toolbar_->location();
	addToolBar(toolbar_);
	
	notepad_.stack = new QStackedWidget();
	setCentralWidget(notepad_.stack);
	notepad_.editor = new gui::TextEdit(this);
	notepad_.editor_index = notepad_.stack->addWidget(notepad_.editor);
	
	main_splitter_ = new QSplitter(Qt::Horizontal);
	notepad_.window_index = notepad_.stack->addWidget(main_splitter_);
	notepad_.stack->setCurrentIndex(notepad_.window_index);
	
	{
		side_pane_model_ = new gui::SidePaneModel(this);
		side_pane_ = new gui::SidePane(side_pane_model_, this);
		side_pane_model_->SetSidePane(side_pane_);
		main_splitter_->addWidget(side_pane_);
		{
			auto &items = side_pane_items();
			MutexGuard guard(&items.mutex);
			items.widgets_created = true;
			int status = pthread_cond_signal(&items.cond);
			if (status != 0)
				mtl_warn("pthread_cond_signal: %s", strerror(status));
		}
	}
	
	{
		table_model_ = new gui::TableModel(this);
		table_ = new gui::Table(table_model_, this);
		main_splitter_->addWidget(table_);
		{
			MutexGuard guard(&view_files_.mutex);
			view_files_.data.widgets_created = true;
			int status = pthread_cond_signal(&view_files_.cond);
			if (status != 0)
				mtl_warn("%s", strerror(status));
		}
	}
	
	main_splitter_->setStretchFactor(0, 0);
	main_splitter_->setStretchFactor(1, 1);
	//main_splitter_->setSizes({170, 1000});
}

void App::DisplayFileContents(const int row, io::File *cloned_file)
{
	if (row == -1) {
		HideTextEditor();
		return;
	}
	
	if (cloned_file == nullptr)
		cloned_file = table_->GetFileAt(row);
	if (cloned_file == nullptr)
		return;
	
	if (notepad_.editor->Display(cloned_file))
		notepad_.stack->setCurrentIndex(notepad_.editor_index);
}

void App::DisplaySymlinkInfo(io::File &file)
{
	io::LinkTarget *t = file.link_target();
	CHECK_PTR_VOID(t);
	
	QDialog dialog(this);
	dialog.setWindowTitle("Symlink path chain");
	QFormLayout *layout = new QFormLayout();
	dialog.setLayout(layout);
	QFont font;
	QFontMetricsF font_metrics(font);
	int max_len = 20;
	
	for (const QString &next: t->chain_paths_) {
		QRectF rect = font_metrics.boundingRect(next);

		if (rect.width() > max_len) {
			max_len = rect.width() + 50;
		}
	}
	
	// 300 <= width <= 900
	max_len = std::min(900, std::max(350, max_len));
	static auto go_icon = QIcon::fromTheme(QLatin1String("go-jump"));
	
	for (auto &next: t->chain_paths_) {
		QLineEdit *input = new QLineEdit();
		input->setReadOnly(true);
		input->setFixedWidth(max_len);
		input->setText(next);
		
		auto *go_btn = new QPushButton(go_icon, QString());
		go_btn->setToolTip("Go to this file");
		connect(go_btn, &QPushButton::clicked, [&dialog, this, next]() {
			dialog.close();
			GoToAndSelect(next);
		});
		
		layout->addRow(input, go_btn);
	}
	
	dialog.exec();
}

void App::FileDoubleClicked(io::File *file, const gui::Column col)
{
	cornus::AutoDelete ad(file);

	if (col == gui::Column::Icon) {
		if (file->is_symlink()) {
			DisplaySymlinkInfo(*file);
		} else {
			mtl_info("Display access permissions");
		}
	} else if (col == gui::Column::FileName) {
		if (file->is_dir()) {
			QString full_path = file->build_full_path();
			GoTo({full_path, false});
		} else if (file->is_link_to_dir()) {
			if (file->link_target() != nullptr) {
				GoTo({file->link_target()->path, true});
			}
		} else if (file->is_regular() || file->is_symlink()) {
			QString full_path = file->build_full_path();
			ExecInfo info = QueryExecInfo(full_path, file->cache().ext.toString());
			if (info.is_elf() || info.is_script()) {
				RunExecutable(full_path, info);
			} else {
				QUrl url = QUrl::fromLocalFile(full_path);
				QDesktopServices::openUrl(url);
			}
		}
	}
}

QIcon* App::GetIcon(const QString &str) {
	QString s = GetIconName(str);
	
	if (s.isEmpty())
		return nullptr;
	
	return GetOrLoadIcon(s);
}

QString App::GetIconName(const QString &trunc) {
	for (const auto &name: available_icon_names_) {
		if (name.startsWith(trunc)) {
			return name;
		}
	}
	
	return QString();
}

QIcon* App::GetOrLoadIcon(const QString &icon_name) {
	QIcon *found = icon_set_.value(icon_name, nullptr);
	
	if (found != nullptr)
		return found;
	
	const QString full_path = icons_dir_ + '/' + icon_name;
	auto *icon = new QIcon(full_path);
	icon_set_.insert(icon_name, icon);
	
	return icon;
}

void App::GoBack() {
	mtl_info();
}

void App::GoHome() { GoTo({QDir::homePath(), false}); }

bool App::GoTo(DirPath dp, bool reload, QString scroll_to_and_select)
{
	GoToParams *params = new GoToParams();
	params->app = this;
	params->dir_path = dp;
	params->reload = reload;
	params->scroll_to_and_select = scroll_to_and_select;
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::GoToTh, params);
	if (status != 0)
		mtl_warn("pthread_create: %s", strerror(errno));
	
	return true;
}

void App::GoToFinish(cornus::io::FilesData *new_data)
{
	AutoDelete ad(new_data);
	int count = new_data->vec.size();
	table_model_->SwitchTo(new_data);
	current_dir_ = new_data->processed_dir_path;
	
	if (new_data->scroll_to_and_select.isEmpty())
		table_->ScrollToRow(0);
	
	QString dir_name = QDir(new_data->processed_dir_path).dirName();
	if (dir_name.isEmpty())
		dir_name = new_data->processed_dir_path; // likely "/"
	
	using Clock = std::chrono::steady_clock;
	
	if (new_data->start_time != std::chrono::time_point<Clock>::max())
	{
		auto now = std::chrono::steady_clock::now();
		const float elapsed = std::chrono::duration<float,
			std::chrono::milliseconds::period>(now - new_data->start_time).count();
		QString diff_str = io::FloatToString(elapsed, 2);
		
		QString title = dir_name
			+ QLatin1String(" (") + QString::number(count)
			+ QChar('/') + diff_str + QLatin1String(" ms)");
		setWindowTitle(title);
	} else {
		setWindowTitle(dir_name);
	}
	
	location_->SetLocation(new_data->processed_dir_path);
	side_pane_->SelectProperPartition(new_data->processed_dir_path);
}

void App::GoToInitialDir()
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
	
	for (int i = 1; i < arg_count; i++) {
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
	
	if (!cmds.go_to_path.isEmpty()) {
		io::FileType file_type;
		if (!io::FileExists(cmds.go_to_path, &file_type)) {
			QString msg = "Directory doesn't exist:\n\""
				+ cmds.go_to_path + QChar('\"');
			mtl_printq(msg);
			GoTo({QDir::homePath(), false}, false, cmds.select);
			return;
		}
		if (file_type == io::FileType::Dir) {
			GoTo({cmds.go_to_path, false}, false, cmds.select);
		} else {
			QDir parent_dir(cmds.go_to_path);
			if (!parent_dir.cdUp()) {
				QString msg = "Can't access parent dir of file:\n\"" +
					cmds.go_to_path + QChar('\"');
				mtl_printq(msg);
				GoHome();
				return;
			}
			GoTo({parent_dir.absolutePath(), false}, false, cmds.go_to_path);
			return;
		}
	}
}

void App::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	CHECK_TRUE_VOID(parent.exists());
	QString parent_dir = parent.absolutePath();
	
	if (!io::SameFiles(parent_dir, current_dir_)) {
		CHECK_TRUE_VOID(GoTo({parent_dir, false}, false, full_path));
	} else {
		table_->ScrollToAndSelect(full_path);
	}
}

void App::GoUp()
{
	if (current_dir_.isEmpty())
		return;
	
	QDir dir(current_dir_);
	
	if (!dir.cdUp())
		return;
	
	QString select_path = current_dir_;
	GoTo({dir.absolutePath(), true}, false, select_path);
}

void App::HideTextEditor() {
	notepad_.stack->setCurrentIndex(notepad_.window_index);
}

void App::IconByTruncName(io::File &file, const QString &truncated, QIcon **ret_icon) {
	QString real_name = GetIconName(truncated);
	CHECK_TRUE_VOID((!real_name.isEmpty()));
	auto *icon = GetOrLoadIcon(real_name);
	file.cache().icon = icon;
	
	if (ret_icon != nullptr)
		*ret_icon = icon;
}

void App::IconByFileName(io::File &file, const QString &filename, QIcon **ret_icon)
{
	auto *icon = GetOrLoadIcon(filename);
	file.cache().icon = icon;
	
	if (ret_icon != nullptr)
		*ret_icon = icon;
}

void App::LoadIcon(io::File &file)
{
	if (file.is_dir_or_so()) {
		if (icon_cache_.folder == nullptr)
			IconByTruncName(file, FolderIconName, &icon_cache_.folder);
		else
			file.cache().icon = icon_cache_.folder;
		return;
	}
	
	const auto &fn = file.name_lower();
	const auto &ext = file.cache().ext;
	
	if (ext.isEmpty()) {
		
		if (fn.indexOf(QLatin1String(".so.")) != -1)
		{
			if (icon_cache_.lib == nullptr)
				IconByTruncName(file, QLatin1String("special_sharedlib"), &icon_cache_.lib);
			else
				file.cache().icon = icon_cache_.lib;
			return;
		}
		
		SetDefaultIcon(file);
		return;
	}
	
	QString filename = icon_names_.value(ext.toString());
	
	if (!filename.isEmpty()) {
		IconByFileName(file, filename);
		return;
	}
	
	if (ext.startsWith(QLatin1String("blend"))) {
		QString trunc = QLatin1String("special_blender");
		IconByTruncName(file, trunc);
		return;
	}
	
	SetDefaultIcon(file);
}

void App::OpenTerminal() {
	const QString konsole_path = QLatin1String("/usr/bin/konsole");
	const QString gnome_terminal_path = QLatin1String("/usr/bin/gnome-terminal");
	const QString *path = nullptr;
	
	if (io::FileExists(konsole_path)) {
		path = &konsole_path;
	} else if (io::FileExists(gnome_terminal_path)) {
		path = &gnome_terminal_path;
	} else {
		return;
	}
	
	QStringList arguments;
	QProcess::startDetached(*path, arguments, current_dir_);
}

ExecInfo App::QueryExecInfo(io::File &file) {
	return QueryExecInfo(file.build_full_path(), file.cache().ext.toString());
}

ExecInfo App::QueryExecInfo(const QString &full_path, const QString &ext)
{
/// ls -ls ./2to3-2.7 
/// 4 -rwxr-xr-x 1 root root 96 Aug 24 22:12 ./2to3-2.7

///#! /bin/sh
	const isize size = 32;
	char buf[size];
	ExecInfo ret = {};
	
	if (io::TryReadFile(full_path, buf, size, &ret) < size)
		return ret;
	
	if (!ext.isEmpty()) {
		if (ext == QLatin1String("sh")) {
			ret.type |= (ExecType::Script | ExecType::ScriptSh);
		} else if (ext == QLatin1String("py")) {
			ret.type |= (ExecType::Script | ExecType::ScriptPython);
		} else if (ext == QLatin1String("bat")) {
			ret.type |= ExecType::ScriptBat;
		}
	}
	
	if(ext.isEmpty()) {
		TestExecBuf(buf, size, ret);
	}
	
	return ret;
}

bool App::TestExecBuf(const char *buf, const isize size, ExecInfo &ret)
{
	// returns true if no more querying is needed
	
	if (buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') {
		ret.type |= ExecType::Elf;
		return true;
	}
	
	if (buf[0] == '#' && buf[1] == '!') {
		ret.type |= ExecType::Script;
		QString s = QString::fromLocal8Bit(buf, size);
		const QString bin = QLatin1String("bin/");
		int index = s.indexOf(bin);
		if (index == -1)
			return true;

		QStringRef ref = s.midRef(index + bin.size());
		if (ref.startsWith(QLatin1String("sh"))) {
			ret.type |= ExecType::ScriptSh;
			return true;
		} else if (ref.startsWith(QLatin1String("python"))) {
			ret.type |= ExecType::ScriptPython;
			return true;
		} else if (ref.startsWith(QLatin1String("bash"))) {
			ret.type |= ExecType::ScriptBash;
			return true;
		}
	}
	
	return false;
}

void App::RegisterShortcuts() {
	auto *shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Up), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	connect(shortcut, &QShortcut::activated, this, &App::GoUp);
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_H), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		prefs_.show_hidden_files = !prefs_.show_hidden_files;
		GoTo({current_dir_, true}, true);
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		QApplication::quit();
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		table_model_->DeleteSelectedFiles();
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		RenameSelectedFile();
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_I), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		table_->setFocus();
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_L), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		location_->setFocus();
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_A), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		table_->setFocus();
		QVector<int> indices;
		MutexGuard guard(&view_files_.mutex);
		table_->SelectAllFilesNTS(true, indices);
		table_model_->UpdateIndices(indices);
	});
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_E), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		SwitchExecBitOfSelectedFiles();
	});
}

void App::RenameSelectedFile()
{
	io::File *file = nullptr;
	if (table_->GetFirstSelectedFile(&file) == -1)
		return;
	
	AutoDelete ad(file);
	const QString text = file->name();
	
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Rename File"));
	dialog.setModal(true);
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	dialog.setLayout(vert_layout);
	
	{ // icon + mime row
		QBoxLayout *row = new QBoxLayout(QBoxLayout::LeftToRight);
		
		LoadIcon(*file);
		QLabel *icon_label = new QLabel();
		const QIcon *icon = file->cache().icon;
		QPixmap pixmap = icon->pixmap(QSize(48, 48));
		icon_label->setPixmap(pixmap);
		row->addWidget(icon_label);
		
		QString mime;
		if (file->is_symlink()) {
			mime = QLatin1String("inode/symlink");
		} else {
			QString full_path = file->build_full_path();
			QMimeType mt = mime_db_.mimeTypeForFile(full_path);
			mime = mt.name();
		}
		QLabel *text_label = new QLabel();
		text_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		text_label->setText("(<b>" + mime + "</b>)");
		row->addWidget(text_label, 2);
		
		vert_layout->addLayout(row);
	}
	
	QLineEdit *le = new QLineEdit();
	{ // file name row
		vert_layout->addWidget(le);
		le->setText(text);
		
		const QString tar = QLatin1String(".tar.");
		int index = text.lastIndexOf(tar);
		if (index > 0) {
			le->setSelection(0, index);
		} else {
			index = text.lastIndexOf('.');
			if (index > 0 && index < (text.size() - 1)) {
				le->setSelection(0, index);
			} else {
				le->setSelection(0, text.size());
			}
		}
	}
	
	{ // ok + cancel buttons row
		QDialogButtonBox *button_box = new QDialogButtonBox
			(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		vert_layout->addWidget(button_box);
	}
	
	QFontMetrics fm = table_->fontMetrics();
	int w = fm.boundingRect(text).width();
	w = std::min(800, std::max(w + 80, 400)); // between 300 and 800
	dialog.resize(w, 100);
	bool ok = dialog.exec();
	if (!ok)
		return;
	
	QString value = le->text().trimmed();
	if (value.isEmpty())
		return;
	
	auto old_path = file->build_full_path().toLocal8Bit();
	auto new_path = (file->dir_path() + value).toLocal8Bit();
	
	if (old_path == new_path)
		return;
	
	table_model_->set_scroll_to_and_select(new_path);
	
	if (rename(old_path.data(), new_path.data()) != 0) {
		QString err = QString("Failed: ") + strerror(errno);
		QMessageBox::warning(this, "Failed", err);
		table_model_->set_scroll_to_and_select(QString());
	}
}

void App::RunExecutable(const QString &full_path,
	const ExecInfo &info)
{
	if (false) {
		if (info.is_elf())
			mtl_info("ELF executable");
		if (info.is_script())
			mtl_info("Script");
		if (info.is_script_sh())
			mtl_info("Script SH");
		if (info.is_script_bash())
			mtl_info("Script Bash");
		if (info.is_script_python())
			mtl_info("Script Python");
		if (info.has_exec_bit())
			mtl_info("Has exec bit");
		
		if (info.is_regular_file())
			mtl_info("Regular");
		if (info.is_symlink())
			mtl_info("Symlink");
	}
	
	if (info.has_exec_bit() && info.is_elf()) {
		QStringList arguments;
		QProcess::startDetached(full_path, arguments, current_dir_);
	}
}

void App::SaveBookmarks()
{
	QVector<gui::SidePaneItem*> item_vec;
	{
		auto &items = side_pane_items();
		MutexGuard guard(&items.mutex);
		
		for (gui::SidePaneItem *next: items.vec) {
			if (next->is_bookmark()) {
				item_vec.append(next->Clone());
			}
		}
	}
	
	QString parent_dir = prefs::QueryAppConfigPath();
	parent_dir.append('/');
	CHECK_TRUE_VOID(!parent_dir.isEmpty());
	const QString full_path = parent_dir + prefs::BookmarksFileName;
	const QByteArray path_ba = full_path.toLocal8Bit();
	
	if (!io::FileExists(full_path)) {
		int fd = open(path_ba.data(), O_RDWR | O_CREAT | O_EXCL, 0664);
		if (fd == -1) {
			if (errno == EEXIST) {
				mtl_warn("File already exists");
			} else {
				mtl_warn("Can't create file at: \"%s\", reason: %s",
					path_ba.data(), strerror(errno));
			}
			return;
		} else {
			::close(fd);
		}
	}
	
	ByteArray buf;
	buf.add_u16(prefs::BookmarksFormatVersion);
	
	for (gui::SidePaneItem *next: item_vec) {
		buf.add_u8(u8(next->type()));
		buf.add_string(next->mount_path());
		buf.add_string(next->bookmark_name());
	}
	
	if (io::WriteToFile(full_path, buf.data(), buf.size()) != io::Err::Ok) {
		mtl_trace("Failed to save bookmarks");
	}
}

void App::SetDefaultIcon(io::File &file) {
	if (icon_cache_.unknown == nullptr) {
		IconByTruncName(file, QLatin1String("text"), &icon_cache_.unknown);
	} else {
		file.cache().icon = icon_cache_.unknown;
	}
}

void App::SetupIconNames() {
	// .exe, .rpm, .run, .iso
	
	// in /media/data/Documents/BitcoinData/:
	// .log = text/x-log
	// .dat = application/octet-stream
	
	QDir dir(QCoreApplication::applicationDirPath());
	const QString folder_name = QLatin1String("file_icons");
	
	if (dir.exists(folder_name)) {
		icons_dir_ = dir.absoluteFilePath(folder_name);
	} else {
		if (!dir.cdUp()) {
			mtl_trace();
			return;
		}
	
		if (dir.exists(folder_name))
			icons_dir_ = dir.absoluteFilePath(folder_name);
	}
	
	if (io::ListFileNames(icons_dir_, available_icon_names_) != io::Err::Ok) {
		mtl_trace();
	}
	
	for (const auto &name: available_icon_names_) {
		int index = name.lastIndexOf('.');
		if (index == -1)
			icon_names_.insert(name, name);
		else
			icon_names_.insert(name.mid(0, index), name);
	}
}

void App::SetupEnvSearchPaths()
{
	{
		DConfClient *dc = dconf_client_new();
		const gchar *p = "/org/gnome/desktop/interface/icon-theme";
		GVariant *v = dconf_client_read(dc, p);
		gchar *result;
		g_variant_get (v, "s", &result);
		theme_name_ = result;
		g_free (result);
		g_variant_unref(v);
	}
	
//	mtl_printq2("Theme name: ", theme_name_);
	
	auto env = QProcessEnvironment::systemEnvironment();
	{
		QString xdg_data_dirs = env.value(QLatin1String("XDG_DATA_DIRS"));
		
		if (xdg_data_dirs.isEmpty())
			xdg_data_dirs = QLatin1String("/usr/local/share/:/usr/share/");
		
		auto list = xdg_data_dirs.splitRef(':');
		
		for (const auto &s: list) {
			xdg_data_dirs_.append(s.toString());
		}
	}
	
	{
		const QString icons = QLatin1String("icons");
		QString dir_path = QDir::homePath() + '/' + QLatin1String(".icons");
		
		if (io::FileExists(dir_path))
			search_icons_dirs_.append(dir_path);
		
		for (const auto &xdg: xdg_data_dirs_) {
			auto next = QDir(xdg).filePath(icons);
			
			if (io::FileExists(next))
				search_icons_dirs_.append(next);
		}
		
		const char *usp = "/usr/share/pixmaps";
		if (io::FileExistsCstr(usp))
			search_icons_dirs_.append(usp);
		
//		for (auto &s: search_icons_dirs_) {
//			mtl_printq(s);
//		}
	}
	
}

void App::SwitchExecBitOfSelectedFiles() {
	MutexGuard guard(&view_files_.mutex);
	const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;
	for (io::File *next: view_files_.data.vec) {
		if (next->selected()) {
			mode_t mode = next->mode();
			if (mode & ExecBits)
				mode &= ~ExecBits;
			else
				mode |= ExecBits;
			
			auto ba = next->build_full_path().toLocal8Bit();
			if (chmod(ba.data(), mode) != 0) {
				mtl_warn("chmod error: %s", strerror(errno));
			}
		}
	}
}

void App::TellUser(const QString &msg, const QString title) {
	QMessageBox::warning(this, title, msg);
}

bool App::ViewIsAt(const QString &dir_path) const
{
	QString old_dir_path;
	{
		MutexGuard guard(&view_files_.mutex);
		old_dir_path = view_files_.data.processed_dir_path;
	}
	
	if (old_dir_path.isEmpty()) {
		return false;
	}
	
	return io::SameFiles(dir_path, old_dir_path);
}

}
