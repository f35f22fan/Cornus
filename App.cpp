///extern "C" {
/// This has to be the first include otherwise
/// gdbusintrospection.h causes an error.
///	#include <dconf.h>
///	#include <udisks/udisks.h>
///}
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>

#include "App.hpp"
#include "AutoDelete.hh"
#include "DesktopFile.hpp"
#include "ElapsedTimer.hpp"
#include "ExecInfo.hpp"
#include "History.hpp"
#include "io/disks.hh"
#include "io/io.hh"
#include "io/File.hpp"
#include "io/socket.hh"
#include "gui/actions.hxx"
#include "gui/Location.hpp"
#include "gui/SearchPane.hpp"
#include "gui/sidepane.hh"
#include "gui/Tab.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "gui/TextEdit.hpp"
#include "gui/ToolBar.hpp"
#include "gui/TreeItem.hpp"
#include "gui/TreeModel.hpp"
#include "gui/TreeView.hpp"
#include "Media.hpp"
#include "prefs.hh"
#include "Prefs.hpp"
#include "str.hxx"

#include <QApplication>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
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
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>

#include <glib.h>

#include "io/uring.hh"

namespace cornus {

void* AutoLoadServerIfNeeded(void *arg)
{
	pthread_detach(pthread_self());
	ByteArray ba;
	ba.set_msg_id(io::Message::CheckAlive);
	
	if (io::socket::SendSync(ba)) {
		return nullptr;
	}
	
	QString excl_file_path = QDir::homePath()
		+ QLatin1String("/.cornus_check_online_excl_");
	auto excl_ba = excl_file_path.toLocal8Bit();
	int fd = open(excl_ba.data(), O_EXCL | O_CREAT, 0x777);
	
	if (fd == -1) {
		mtl_info("Some app already trying to start cornus_io");
		return nullptr;
	}
	
///	mtl_info("Starting io server..");
	QString server_dir_path = QCoreApplication::applicationDirPath();
	QString server_full_path = server_dir_path + QLatin1String("/cornus_io");
	
	QStringList arguments;
	QProcess::startDetached(server_full_path, arguments, server_dir_path);
///	mtl_info("done");
	int sec = 0;
	for (;sec < 7; sec++) {
		if (io::socket::SendSync(ba))
			break;
		sleep(1);
	}
///	mtl_info("Removing excl file after %d sec of waiting", sec);
	int status = remove(excl_ba.data());
	if (status != 0)
		mtl_status(errno);
	
	return nullptr;
}

void MountAdded(GVolumeMonitor *monitor, GMount *mount, gpointer user_data)
{
	char *name = g_mount_get_name(mount);
	GFile *file = g_mount_get_root(mount);
	GVolume *vol = g_mount_get_volume(mount);
	char *path = g_file_get_path(file);
	char *uuid = g_volume_get_uuid(vol);
//	mtl_info("added, uuid: %s", uuid);
	QString uuid_str = uuid;
	QString path_str = path;
	g_free(uuid);
	g_free(name);
	g_free(path);
	g_object_unref(file);
	
	App *app = (App*)user_data;
	app->tree_model()->MountEvent(path_str, uuid_str, PartitionEventType::Mount);
}

void MountRemoved(GVolumeMonitor *monitor, GMount *mount, gpointer user_data)
{
	char *name = g_mount_get_name(mount);
	GFile *file = g_mount_get_root(mount);
	char *path = g_file_get_path(file);
	QString path_str = path;
	g_free(name);
	g_free(path);
	g_object_unref(file);
	
	App *app = (App*)user_data;
	app->tree_model()->MountEvent(path_str, QString(), PartitionEventType::Unmount);
}

App::App()
{
	qRegisterMetaType<cornus::io::File*>();
	qRegisterMetaType<cornus::io::FilesData*>();
	qRegisterMetaType<cornus::io::FileEvent>();
	qRegisterMetaType<cornus::PartitionEvent*>();
	qRegisterMetaType<cornus::io::CountRecursiveInfo*>();
	qRegisterMetaType<QVector<cornus::gui::TreeItem*>>();
	qDBusRegisterMetaType<QMap<QString, QVariant>>();
	media_ = new Media();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, gui::sidepane::LoadItems, this);
	if (status != 0)
		mtl_status(status);
	
	status = pthread_create(&th, NULL, AutoLoadServerIfNeeded, this);
	if (status != 0)
		mtl_status(status);
	
	setWindowIcon(QIcon(cornus::AppIconPath));
	prefs_ = new Prefs(this);
	prefs_->Load();
	
	{
		tab_widget_ = new QTabWidget();
		tab_widget_->setTabsClosable(true);
		tab_widget_->setTabBarAutoHide(true);
		connect(tab_widget_, &QTabWidget::tabCloseRequested, this, &App::DeleteTabAt);
		connect(tab_widget_, &QTabWidget::currentChanged, this, &App::TabSelected);
		OpenNewTab(FirstTime::Yes);
	}
	
	SetupIconNames();
	io::InitEnvInfo(desktop_, search_icons_dirs_, xdg_data_dirs_, possible_categories_);
	CreateGui();
	if (prefs_->remember_window_size()) {
		QSize sz = prefs_->window_size();
		if (sz.width() < 0 || sz.height() < 0)
			sz = QSize(800, 600);
		resize(sz);
	}
	
	auto *clipboard = QGuiApplication::clipboard();
	connect(clipboard, &QClipboard::changed, this, &App::ClipboardChanged);
	
	ClipboardChanged(QClipboard::Clipboard);
	DetectThemeType();
	RegisterVolumesListener();
	RegisterShortcuts();
	ReadMTP();
/// enables receiving ordinary mouse events (when mouse is not down)
//	setMouseTracking(true);
	
	if (false) {
		pthread_t th;
		int status = pthread_create(&th, NULL, &uring::DoTest, NULL);
		if (status != 0)
			mtl_status(status);
	}
}

App::~App()
{
	QHashIterator<QString, QIcon*> i(icon_set_);
	while (i.hasNext()) {
		i.next();
		QIcon *icon = i.value();
		delete icon;
	}
	{
		tree_data_.Lock();
		tree_data_.sidepane_model_destroyed = true;
		for (auto *item: tree_data_.roots)
			delete item;
		
		tree_data_.roots.clear();
		tree_data_.Unlock();
	}
	{
		/// table_ must be deleted before prefs_ because table_model_ calls 
		/// into prefs().show_free_partition_space() in TableModel::GetName()
		delete tab_widget_;
		delete prefs_;
		prefs_ = nullptr;
	}
	
	delete media_;
	media_ = nullptr;
}

void App::ArchiveAskDestArchivePath(const QString &ext)
{
	QUrl url = QFileDialog::getExistingDirectoryUrl(this,
		tr("Archive destination folder"),
		QUrl::fromLocalFile(tab()->current_dir()));
	
	if (url.isEmpty())
		return;
	
	QString to = url.toLocalFile();
	ArchiveTo(to, ext);
}

void App::ArchiveTo(const QString &dir_path, const QString &ext)
{
	QVector<QString> urls;
	tab()->table()->GetSelectedFileNames(urls);
	if (urls.isEmpty())
		return;
	
	QString files_dir = tab()->current_dir();
	if (!files_dir.endsWith('/'))
		files_dir.append('/');
	
	for (int i = 0; i < urls.size(); i++) {
		QString s = QUrl(files_dir + urls[i]).toString();
		urls[i] = s;
	}
	
	QProcess process;
	process.setWorkingDirectory(dir_path);
	process.setProgram(QLatin1String("ark"));
	QStringList args;
	
	args.append(QLatin1String("-c"));
	args.append(QLatin1String("-f"));
	args.append(ext);
	
	for (const auto &next: urls) {
		args.append(next);
	}
	
	process.setArguments(args);
	process.startDetached();
}

void App::AskCreateNewFile(io::File *file, const QString &title)
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
		
		QLabel *icon_label = new QLabel();
		const QIcon *icon = GetFileIcon(file);
		if (icon != nullptr) {
			QPixmap pixmap = icon->pixmap(QSize(64, 64));
			icon_label->setPixmap(pixmap);
		}
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
	
	QFontMetrics fm = tab()->table()->fontMetrics();
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
	const QString new_path = file->build_full_path();
	auto path_ba = new_path.toLocal8Bit();
	tab()->table_model()->SelectFilenamesLater({file->name()}, gui::SameDir::Yes);
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

void App::ClipboardChanged(QClipboard::Mode mode)
{
	if (mode != QClipboard::Clipboard)
		return;
	
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mime = clipboard->mimeData();
	 // mimeData can be 0 according to https://bugs.kde.org/show_bug.cgi?id=335053
	if (!mime) {
		return;
	}
#ifdef DEBUG_CLIPBOARD
	mtl_info("Clipboard changed");
#endif
	io::GetClipboardFiles(*mime, clipboard_);
	
	if (!clipboard_.has_files())
		return;
	
	QVector<int> indices;
	tab()->table()->SyncWith(clipboard_, indices);
	tab()->table_model()->UpdateIndices(indices);
}

void App::CreateGui()
{
	toolbar_ = new gui::ToolBar(this);
	location_ = toolbar_->location();
	addToolBar(toolbar_);
	
	notepad_.stack = new QStackedWidget();
	setCentralWidget(notepad_.stack);
	
	main_splitter_ = new QSplitter(Qt::Horizontal);
	notepad_.window_index = notepad_.stack->addWidget(main_splitter_);
	notepad_.stack->setCurrentIndex(notepad_.window_index);
	
	CreateSidePane();
	
	{
		tree_data_.Lock();
		tree_data_.widgets_created = true;
		int status = pthread_cond_broadcast(&tree_data_.cond);
		tree_data_.Unlock();
		if (status != 0)
			mtl_status(status);
	}
	
	CreateFilesViewPane();
	
	main_splitter_->setStretchFactor(0, 0);
	main_splitter_->setStretchFactor(1, 1);
	auto &sizes = prefs_->splitter_sizes();
	if (sizes.size() > 0)
		main_splitter_->setSizes(sizes);

	prefs_->UpdateTableSizes();
	
	tab()->GrabFocus();
	{
		QIcon icon = QIcon::fromTheme(QLatin1String("window-new"));
		QToolButton *btn = new QToolButton();
		btn->setIcon(icon);
		connect(btn, &QAbstractButton::clicked, [=] () {
			OpenNewTab();
		});
		tab_widget_->setCornerWidget(btn, Qt::TopRightCorner);
	}
}

void App::CreateFilesViewPane()
{
	QWidget *table_pane = new QWidget();
	QBoxLayout *vlayout = new QBoxLayout(QBoxLayout::TopToBottom);
	vlayout->setContentsMargins(0, 0, 0, 0);
	vlayout->setSpacing(0);
	table_pane->setLayout(vlayout);
	
	vlayout->addWidget(tab_widget_);
	
	search_pane_ = new gui::SearchPane(this);
	vlayout->addWidget(search_pane_);
	search_pane_->setVisible(false);
	
	main_splitter_->addWidget(table_pane);
	{
		MutexGuard guard = gui_bits_.guard();
		gui_bits_.created(true);
		gui_bits_.Broadcast();
	}
}

QMenu* App::CreateNewMenu()
{
	QMenu *menu = new QMenu(tr("Create &New"), this);
	
	{
		QAction *action = menu->addAction(tr("Folder"));
		auto *icon = GetFolderIcon();
		if (icon != nullptr)
			action->setIcon(*icon);
		connect(action, &QAction::triggered, [=] {
			AskCreateNewFile(io::File::NewFolder(tab()->current_dir(), "New Folder"),
				tr("Create New Folder"));
		});
	}
	
	{
		QAction *action = menu->addAction(tr("Text File"));
		auto *icon = GetIcon(QLatin1String("text"));
		if (icon != nullptr)
			action->setIcon(*icon);
		
		connect(action, &QAction::triggered, [=] {
			AskCreateNewFile(io::File::NewTextFile(tab()->current_dir(), "File.txt"),
				tr("Create New File"));
		});
	}
	
	QString config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
	
	if (!config_path.endsWith('/'))
		config_path.append('/');
	
	QString full_path = config_path.append(QLatin1String("Templates"));
	QVector<QString> names;
	io::ListFileNames(full_path, names);
	std::sort(names.begin(), names.end());
	
	if (!names.isEmpty())
		menu->addSeparator();
	
	QString dir_path = tab()->current_dir();
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	for (const auto &name: names)
	{
		QString from_full_path = full_path + '/' + name;
		
		QString ext = io::GetFileNameExtension(name).toString();
		if (ext.isEmpty())
			continue;
		
		QAction *action = new QAction(name);
		QIcon *icon = GetIcon(ext);
		if (icon != nullptr)
			action->setIcon(*icon);
		
		connect(action, &QAction::triggered, [=] {
			tab()->table_model()->SelectFilenamesLater({name}, gui::SameDir::Yes);
			io::CopyFileFromTo(from_full_path, dir_path);
		});
		menu->addAction(action);
	}
	
	return menu;
}

void App::CreateSidePane()
{
	tree_model_ = new gui::TreeModel(this);
	tree_view_ = new gui::TreeView(this, tree_model_);
	tree_model_->SetView(tree_view_);
	main_splitter_->addWidget(tree_view_);
}

i32 App::current_dir_id() const
{
	gui::Tab *tab = this->tab();
	auto *view_files = files(tab->files_id());
	auto guard = view_files->guard();
	return view_files->data.dir_id;
}

void App::DeleteFilesById(const i64 id)
{
	io::Files *p = files_.value(id, nullptr);
	if (p != nullptr)
	{
		files_.remove(id);
		delete p;
	}
}

void App::DeleteTabAt(const int i)
{
	auto *tab = tab_widget_->widget(i);
	const bool do_exit = (tab_widget_->count() == 1);
	tab_widget_->removeTab(i);
	if (do_exit)
		prefs_->Save();
	delete tab;
	
	if (do_exit)
		QApplication::quit();
}

void App::DetectThemeType()
{
	const QStyleOptionViewItem option = tree_view_->option();
	const QColor c = option.palette.window().color();
	const i32 avg = (c.red() + c.green() + c.blue()) / 3;
	theme_type_ = (avg > 150) ? ThemeType::Light : ThemeType::Dark;
//	mtl_info("avg: %d, light: %s", avg, (theme_type_ == ThemeType::Light)
//		? "true" : "false");
}

void App::DisplayFileContents(const int row, io::File *cloned_file)
{
	if (cloned_file == nullptr) {
		cloned_file = tab()->table()->GetFileAt(row);
		if (cloned_file == nullptr)
			return;
	}
	
	if (notepad_.editor == nullptr) {
		notepad_.editor = new gui::TextEdit(this);
		notepad_.editor_index = notepad_.stack->addWidget(notepad_.editor);
	}
	
	notepad_.saved_window_title = windowTitle();
	if (notepad_.editor->Display(cloned_file))
	{
		setWindowTitle(tr("Press Esc to exit or Ctrl+S to save and exit"));
		toolbar_->setVisible(false);
		notepad_.stack->setCurrentIndex(notepad_.editor_index);
		tab()->table()->ClearMouseOver();
	}
}

void App::DisplaySymlinkInfo(io::File &file)
{
	if (!file.has_link_target())
		file.ReadLinkTarget();
	
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
	
	for (auto &next: t->chain_paths_)
	{
		QLineEdit *input = new QLineEdit();
		input->setReadOnly(true);
		input->setFixedWidth(max_len);
		input->setText(next);
		
		auto *go_btn = new QPushButton(go_icon, QString());
		go_btn->setToolTip("Go to this file");
		connect(go_btn, &QPushButton::clicked, [&dialog, this, next]() {
			dialog.close();
			tab()->GoToAndSelect(next);
		});
		
		layout->addRow(input, go_btn);
	}
	
	dialog.exec();
}

void App::EditSelectedMovieTitle()
{
	QString ext;
	QString full_path = tab()->table()->GetFirstSelectedFileFullPath(&ext);
	if (full_path.isEmpty())
		return;
	
	QString file_name = io::GetFileNameOfFullPath(full_path).toString();
	if (file_name.isEmpty())
		return;
	
	gui::InputDialogParams params;
	params.title = tr("Edit movie title");
	params.msg = file_name;
	params.placeholder_text = tr("Movie title");
	params.icon = GetIcon(ext);
	QString movie_title;
	if (!ShowInputDialog(params, movie_title))
		return;
	
	movie_title = movie_title.trimmed();
	if (movie_title.isEmpty())
		return;

	QProcess process;
	{ // WORKAROUND for mkvpropedit's UTF-8 support. Inspired from:
// https://www.qtcentre.org/threads/5375-Passing-to-a-console-application-(managed-via-QProcess)-UTF-8-encoded-parameters
		auto env = QProcessEnvironment::systemEnvironment();
		env.insert(QLatin1String("LANG"), QLatin1String("en_US.UTF-8"));
		process.setProcessEnvironment(env);
	}
	
	process.setProgram(QLatin1String("mkvpropedit"));
	process.setWorkingDirectory(tab()->current_dir());
	QStringList args;
	args.append(file_name);
	args.append(QLatin1String("--edit"));
	args.append(QLatin1String("info"));
	args.append(QLatin1String("--set"));
	QString t = QLatin1String("title=") + movie_title;
	args.append(QString::fromLocal8Bit(t.toUtf8()));
	
	process.setArguments(args);
	process.startDetached();
}

bool App::event(QEvent *evt)
{
/** NB: Retain the order - first call QWidget::event(evt) and then
 everything else, otherwise DetectThemeType() won't get called
 before the other widgets get repainted with the new theme. */
	const auto ret = QWidget::event(evt);
	switch (evt->type())
	{
	case QEvent::ApplicationPaletteChange: {
		DetectThemeType();
		break;
	}
	default:;
	}
	
	return ret;
}

void App::ExtractAskDestFolder()
{
	QUrl dir = QUrl::fromLocalFile(tab()->current_dir());
	QUrl url = QFileDialog::getExistingDirectoryUrl(this,
		tr("Extract destination folder"), dir);
	
	if (url.isEmpty())
		return;
	
	ExtractTo(url.toLocalFile());
}

void App::ExtractTo(const QString &to_dir)
{
	QVector<QString> urls;
	tab()->table()->GetSelectedArchives(urls);
	if (urls.isEmpty())
		return;
	
	QProcess process;
	process.setWorkingDirectory(to_dir);
	process.setProgram(QLatin1String("ark"));
	QStringList args;
	
	args.append(QLatin1String("-b"));
	args.append(QLatin1String("-a"));
	
	for (const auto &next: urls) {
		args.append(next);
	}
	
	process.setArguments(args);
	process.startDetached();
}

void App::FileDoubleClicked(io::File *file, const gui::Column col)
{
	cornus::AutoDelete ad(file);

	if (col == gui::Column::Icon) {
		if (file->is_symlink()) {
			DisplaySymlinkInfo(*file);
		} else {
			DisplayFileContents(-1, file->Clone());
		}
	} else if (col == gui::Column::FileName) {
		if (file->is_dir()) {
			QString full_path = file->build_full_path();
			tab()->GoTo(Action::To, {full_path, Processed::No});
		} else if (file->is_link_to_dir()) {
			if (file->link_target() != nullptr) {
				tab()->GoTo(Action::To, {file->link_target()->path, Processed::Yes});
			}
		} else if (file->is_regular() || file->is_symlink()) {
			
			QString ext;
			QString full_path;
			bool has_exec_bit = false;
			if (file->is_regular()) {
				ext = file->cache().ext.toString();
				has_exec_bit = file->has_exec_bit();
				full_path = file->build_full_path();
			} else {
				io::LinkTarget *target = file->link_target();
				if (target == nullptr)
					return;
				full_path = target->path;
				has_exec_bit = io::HasExecBit(full_path) == Bool::Yes;
				ext = io::GetFileNameExtension(full_path).toString().toLower();
			}
			
			if (ext == str::Desktop)
			{
				LaunchOrOpenDesktopFile(full_path,
					has_exec_bit, RunAction::ChooseBasedOnExecBit);
				return;
			}
			
			ExecInfo info = QueryExecInfo(full_path, ext);
			if (info.is_elf() || info.is_shell_script()) {
				RunExecutable(full_path, info);
			} else {
				OpenWithDefaultApp(full_path);
			}
		}
	}
}

io::Files* App::files(const i64 files_id) const
{
	return files_.value(files_id, nullptr);
}

i64 App::GenNextFilesId()
{
	next_files_id_++;
	files_.insert(next_files_id_, new io::Files());
	return next_files_id_;
}

QIcon* App::GetDefaultIcon()
{
	if (icon_cache_.unknown == nullptr) {
		QString full_path = GetIconThatStartsWith(QLatin1String("text"));
		CHECK_TRUE_NULL((!full_path.isEmpty()));
		icon_cache_.unknown = GetIconOrLoadExisting(full_path);
	}
	return icon_cache_.unknown;
}

QIcon* App::GetFileIcon(io::File *file)
{
	if (file->cache().icon != nullptr)
		return file->cache().icon;
	
	QIcon *icon = LoadIcon(*file);
	file->cache().icon = icon;
	return icon;
}

QIcon* App::GetFolderIcon()
{
	if (icon_cache_.folder != nullptr)
		return icon_cache_.folder;
	icon_cache_.folder = GetIcon(QLatin1String("special_folder"));
	return icon_cache_.folder;
}

QIcon* App::GetIcon(const QString &str) {
	QString full_path = GetIconThatStartsWith(str);
	return GetIconOrLoadExisting(full_path);
}

QIcon* App::GetIconOrLoadExisting(const QString &icon_path)
{
	if (icon_path.isEmpty())
		return nullptr;
	
	QIcon *found = icon_set_.value(icon_path, nullptr);
	
	if (found != nullptr)
		return found;
	
	const QString full_path = icon_path;///icons_dir_ + '/' + icon_name;
	auto *icon = new QIcon(full_path);
	icon_set_.insert(icon_path, icon);
	///	mtl_info("icon_name: %s", qPrintable(icon_name));
	return icon;
}

QString App::GetIconThatStartsWith(const QString &s)
{
	QHashIterator<QString, QString> it(icon_names_);
	while (it.hasNext()) {
		it.next();
		if (it.key().startsWith(s))
			return it.value();
	}
	
	return QString();
}

QString App::GetPartitionFreeSpace()
{
	QString current_dir = tab()->current_dir();
	if (current_dir.isEmpty())
		return QString();
	
	auto ba = current_dir.toLocal8Bit();
	struct statvfs stv = {};
	int status = statvfs(ba.data(), &stv);
	if (status != 0)
	{
		mtl_warn("%s, file: %s", strerror(errno), ba.data());
		return QString();
	}
	
	const i64 total_space = stv.f_frsize * stv.f_blocks;
	const i64 free_space = stv.f_bavail /*stv.f_bfree*/ * stv.f_bsize;
	
	QString s = io::SizeToString(free_space, StringLength::Short);
	s.append(tr(" free of "));
	s += io::SizeToString(total_space, StringLength::Short);

	return s;
}

void App::GoUp() { tab()->GoUp(); }

void App::HideTextEditor() {
	if (notepad_.stack->currentIndex() == notepad_.window_index)
		return;
	
	setWindowTitle(notepad_.saved_window_title);
	toolbar_->setVisible(true);
	notepad_.stack->setCurrentIndex(notepad_.window_index);
}

QColor App::hover_bg_color_gray(const QColor &c) const
{
	if (theme_type_ == ThemeType::Dark)
		return QColor(90, 90, 90);
	
	QColor n = c.lighter(180);
	const int avg = (n.red() + n.green() + n.blue()) / 3;
	if (avg >= 240)
		return c.lighter(140);
	return n;
}

void App::LaunchOrOpenDesktopFile(const QString &full_path,
	const bool has_exec_bit, const RunAction action) const
{
	bool open = action == RunAction::Open;
	if (!open)
		open = (action == RunAction::ChooseBasedOnExecBit) && !has_exec_bit;
	
	if (open)
	{
		OpenWithDefaultApp(full_path);
		return;
	}
	
	DesktopFile *df = DesktopFile::FromPath(full_path, possible_categories_);
	if (df == nullptr)
		return;
	
	df->LaunchEmpty(tab()->current_dir());
	delete df;
}

QIcon* App::LoadIcon(io::File &file)
{
	if (file.is_dir_or_so()) {
		if (icon_cache_.folder == nullptr) {
			QString full_path = GetIconThatStartsWith(QLatin1String("special_folder"));
			CHECK_TRUE_NULL((!full_path.isEmpty()));
			icon_cache_.folder = GetIconOrLoadExisting(full_path);
		}
		
		return icon_cache_.folder;
	}
	
	const auto &fn = file.name_lower();
	const auto &ext = file.cache().ext;
	
	if (ext.isEmpty()) {
		
		if (fn.indexOf(QLatin1String(".so.")) != -1)
		{
			if (icon_cache_.lib == nullptr) {
				QString full_path = GetIconThatStartsWith(QLatin1String("special_sharedlib"));
				CHECK_TRUE_NULL((!full_path.isEmpty()));
				icon_cache_.lib = GetIconOrLoadExisting(full_path);
			}
			return icon_cache_.lib;
		}
		
		return GetDefaultIcon();
	}
	
	QString filename = icon_names_.value(ext.toString());
	
	if (!filename.isEmpty()) {
		return GetIconOrLoadExisting(filename);
	}
	
	if (ext.startsWith(QLatin1String("blend"))) {
		QString full_path = GetIconThatStartsWith(QLatin1String("special_blender"));
		CHECK_TRUE_NULL((!full_path.isEmpty()));
		return GetIconOrLoadExisting(full_path);
	}
	
	return GetDefaultIcon();
}

void App::LoadIconsFrom(QString dir_path)
{
///	mtl_info("Loading icons from: %s", qPrintable(dir_path));
	QVector<QString> available_names;
	if (io::ListFileNames(dir_path, available_names) != 0) {
		mtl_trace();
	}
	
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	for (const auto &name: available_names)
	{
		int index = name.lastIndexOf('.');
		auto ext = (index == -1) ? name : name.mid(0, index);
		if (!icon_names_.contains(ext)) {
			icon_names_.insert(ext, dir_path + name);
		}
	}
}

void App::MediaFileChanged()
{
	Q_EMIT media_->Changed();
}

gui::Tab* App::OpenNewTab(const cornus::FirstTime ft)
{
	gui::Tab *tab = new gui::Tab(this);
	tab_widget_->addTab(tab, QString());
	if (ft == FirstTime::Yes) {
		tab->GoToInitialDir();
	} else {
		tab->GoHome();
		tab->setVisible(true);
		tab_widget_->setCurrentWidget(tab);
	}
	
	return tab;
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
	QProcess::startDetached(*path, arguments, tab()->current_dir());
}

void App::OpenWithDefaultApp(const QString &full_path) const
{
	ByteArray ba;
	ba.set_msg_id(io::Message::SendDefaultDesktopFileForFullPath);
	ba.add_string(full_path);
	int fd = io::socket::Client();
	CHECK_TRUE_VOID((fd != -1));
	CHECK_TRUE_VOID(ba.Send(fd, false));
	ba.Clear();
	CHECK_TRUE_VOID(ba.Receive(fd));
	CHECK_TRUE_VOID((!ba.is_empty()));
	
	DesktopFile *p = DesktopFile::From(ba);
	CHECK_PTR_VOID(p);
	p->Launch(full_path, tab()->current_dir());
	delete p;
}

ExecInfo App::QueryExecInfo(io::File &file) {
	return QueryExecInfo(file.build_full_path(), file.cache().ext.toString());
}

ExecInfo App::QueryExecInfo(const QString &full_path, const QString &ext)
{
/// ls -ls ./2to3-2.7 
/// 4 -rwxr-xr-x 1 root root 96 Aug 24 22:12 ./2to3-2.7

///#! /bin/sh
	const isize size = 64;
	char buf[size];
	ExecInfo ret = {};
	
	auto real_size = io::TryReadFile(full_path, buf, size, &ret);
	
	if (!ext.isEmpty()) {
		if (ext == QLatin1String("sh") || ext == QLatin1String("py")) {
			ret.type |= ExecType::ShellScript;
		} else if (ext == QLatin1String("bat")) {
			ret.type |= ExecType::BatScript;
		}
	}
	
	if(real_size > 0)
		TestExecBuf(buf, real_size, ret);
	
	return ret;
}

QString App::QueryMimeType(const QString &full_path) {
	QMimeType mt = mime_db_.mimeTypeForFile(full_path);
	return mt.name();
}

/*static void dump_folder_list(LIBMTP_folder_t *folderlist, int level)
{
	if(!folderlist)
		return;
	
	mtl_infon("%u\t", folderlist->folder_id);
	for(int i = 0; i < level; i++) printf("  ");
	
	mtl_infon("%s\n", folderlist->name);
	
	dump_folder_list(folderlist->child, level+1);
	dump_folder_list(folderlist->sibling, level);
} */

int App::ReadMTP()
{
	if (true)
		return 0;
	
	LIBMTP_raw_device_t *rawdevices;
	int numrawdevices;
	int i;
	
	LIBMTP_Init();
	mtl_info("Attempting to connect device(s)");
	const int status = LIBMTP_Detect_Raw_Devices(&rawdevices, &numrawdevices);
	mtl_info("Count: %d", numrawdevices);
	
	switch (status)
	{
	case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
		mtl_warn("No devices found");
		return 0;
	case LIBMTP_ERROR_CONNECTING:
		mtl_warn("There has been an error connecting. Exit");
		return 1;
	case LIBMTP_ERROR_MEMORY_ALLOCATION:
		mtl_warn("Memory Allocation Error. Exit");
		return 1;
		
		/* Successfully connected at least one device, so continue */
	case LIBMTP_ERROR_NONE: {
		mtl_info("Successfully connected: %d", numrawdevices);
		break;
	}
		
		/* Unknown general errors - This should never execute */
	case LIBMTP_ERROR_GENERAL:
	default:
		mtl_warn("mtp-folders: Unknown error, please report "
  "this to the libmtp developers");
		return 1;
	}
mtl_trace();
	/* iterate through connected MTP devices */
	for (i = 0; i < numrawdevices; i++)
	{
mtl_trace();
		LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device(&rawdevices[i]);
mtl_trace();
		if (device == NULL) {
			mtl_warn("Unable to open raw device %d", i);
			continue;
		}
		
		/* Echo the friendly name so we know which device we are working with */
mtl_trace();
		char *friendlyname = LIBMTP_Get_Friendlyname(device);
mtl_trace();
		if (friendlyname == NULL) {
			mtl_info("Friendly name is NULL");
		} else {
			mtl_info("Friendly name: %s", friendlyname);
			free(friendlyname);
		}
mtl_trace();
		LIBMTP_Dump_Errorstack(device);
mtl_trace();
		LIBMTP_Clear_Errorstack(device);
mtl_trace();
		/* Get all storages for this device */
		int ret = LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_NOTSORTED);
		if (ret != 0) {
			mtl_errno();
			LIBMTP_Dump_Errorstack(device);
			LIBMTP_Clear_Errorstack(device);
			continue;
		}
mtl_trace();
		LIBMTP_devicestorage_t *storage = nullptr;
		/* Loop over storages, dump folder for each one */
		for (storage = device->storage; storage != 0; storage = storage->next)
		{
			LIBMTP_folder_t *folders;
			mtl_info("Storage: %s", storage->StorageDescription);
			folders = LIBMTP_Get_Folder_List_For_Storage(device, storage->id);
mtl_trace();
			if (folders == NULL) {
				mtl_info("No folders found");
				LIBMTP_Dump_Errorstack(device);
				LIBMTP_Clear_Errorstack(device);
			} else {
				//dump_folder_list(folders,0);
			}
mtl_trace();
			LIBMTP_destroy_folder_t(folders);
mtl_trace();
		}
mtl_trace();
		LIBMTP_Release_Device(device);
mtl_trace();
	}
	
mtl_trace();
	free(rawdevices);
	mtl_info("Done.");
	
	return 0;
}

void App::RegisterShortcuts()
{
	QShortcut *shortcut;
	{
		shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Up), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		connect(shortcut, &QShortcut::activated, this, &App::GoUp);
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_U), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		connect(shortcut, &QShortcut::activated, [=] {
//			tab()->UndoDelete(0); // 0 = most recent batch, -1 = all
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_O), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		connect(shortcut, &QShortcut::activated, [=] {
			OpenTerminal();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_0), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		connect(shortcut, &QShortcut::activated, [=] {
			prefs_->WheelEventFromMainView(Zoom::Reset);
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_H), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		connect(shortcut, &QShortcut::activated, [=] {
			prefs_->show_hidden_files(!prefs_->show_hidden_files());
			tab()->GoTo(Action::Reload, {tab()->current_dir(), Processed::Yes}, Reload::Yes);
			prefs_->Save();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			search_pane_->SetSearchByFileName();
			search_pane_->setVisible(true);
			search_pane_->RequestFocus();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_M), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			search_pane_->SetSearchByMediaXattr();
			search_pane_->setVisible(true);
			search_pane_->RequestFocus();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			QApplication::quit();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			tab()->table_model()->DeleteSelectedFiles(ShiftPressed::Yes);
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_I), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			tab()->GrabFocus();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_L), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			location_->setFocus();
			location_->selectAll();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_A), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			gui::Tab *tab = this->tab();
			tab->GrabFocus();
			QVector<int> indices;
			auto &view_files = *files(tab->files_id());
			MutexGuard guard = view_files.guard();
			tab->table()->SelectAllFilesNTS(true, indices);
			tab->table_model()->UpdateIndices(indices);
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_E), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			ToggleExecBitOfSelectedFiles();
		});
	}
	{
		shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_T), this);
		shortcut->setContext(Qt::ApplicationShortcut);
		
		connect(shortcut, &QShortcut::activated, [=] {
			OpenNewTab();
		});
	}
}

void App::RegisterVolumesListener()
{
	pthread_t th;
	int status = pthread_create(&th, NULL, gui::sidepane::udev_monitor, this);
	if (status != 0)
		mtl_status(status);
	GVolumeMonitor *monitor = g_volume_monitor_get ();
	g_signal_connect(monitor, "mount-added", (GCallback)MountAdded, this);
	g_signal_connect(monitor, "mount-removed", (GCallback)MountRemoved, this);
	//g_signal_connect(monitor, "mount-changed", (GCallback)MountChanged, this);
}

void App::Reload() {
	tab()->GoTo(Action::Reload, {tab()->current_dir(), Processed::Yes}, Reload::Yes);
}

void App::RenameSelectedFile()
{
	io::File *file = nullptr;
	gui::Tab *tab = this->tab();
	if (tab->table()->GetFirstSelectedFile(&file) == -1)
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
		
		QLabel *icon_label = new QLabel();
		const QIcon *icon = GetFileIcon(file);
		if (icon != nullptr) {
			QPixmap pixmap = icon->pixmap(QSize(48, 48));
			icon_label->setPixmap(pixmap);
		}
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
	
	QFontMetrics fm = tab->table()->fontMetrics();
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
	
	tab->table_model()->SelectFilenamesLater({value});
	
	if (rename(old_path.data(), new_path.data()) != 0) {
		QString err = QString("Failed: ") + strerror(errno);
		QMessageBox::warning(this, "Failed", err);
	}
}

void App::RunExecutable(const QString &full_path, const ExecInfo &info)
{
	if (info.has_exec_bit()) {
		if (info.is_elf()) {
			QStringList args;
			QProcess::startDetached(full_path, args, tab()->current_dir());
		} else if (info.is_shell_script()) {
			info.Run(full_path, tab()->current_dir());
		} else {
			mtl_trace();
		}
	}
}

void App::SaveBookmarks()
{
	QVector<gui::TreeItem*> item_vec;
	gui::TreeItem *bkm_root = tree_data_.GetBookmarksRoot();
	CHECK_PTR_VOID(bkm_root);
	for (gui::TreeItem *next: bkm_root->children())
	{
		item_vec.append(next->Clone());
	}
	
	const QString full_path = prefs::GetBookmarksFilePath();
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
	
	for (gui::TreeItem *next: item_vec) {
		buf.add_u8(u8(next->type()));
		buf.add_string(next->mount_path());
		buf.add_string(next->bookmark_name());
	}
	
	for (auto *next: item_vec) {
		delete next;
	}
	
	{
		tree_data_.Lock();
		tree_data_.bookmarks_changed_by_me = true;
		tree_data_.Unlock();
	}
	if (io::WriteToFile(full_path, buf.data(), buf.size()) != io::Err::Ok) {
		mtl_trace("Failed to save bookmarks");
	}
}

void App::SelectCurrentTab()
{
	TabSelected(tab_widget_->currentIndex());
}

void App::SetupIconNames()
{
	const QString folder_name = QLatin1String("file_icons");
	
	QString icons_from_config = prefs::QueryAppConfigPath();
	if (!icons_from_config.endsWith('/'))
		icons_from_config.append('/');
	icons_from_config.append(folder_name);
	
	if (io::DirExists(icons_from_config)) {
		LoadIconsFrom(icons_from_config);
	} else {
		QDir app_dir(QCoreApplication::applicationDirPath());
		if (app_dir.exists(folder_name)) {
			LoadIconsFrom(app_dir.filePath(folder_name));
		} else if (app_dir.cdUp()) {
			if (app_dir.exists(folder_name))
				LoadIconsFrom(app_dir.filePath(folder_name));
		}
	}
	
	const QString shared_dir = QLatin1String("/usr/share/cornus/") + folder_name;
	if (io::DirExists(shared_dir))
		LoadIconsFrom(shared_dir);
}

bool App::ShowInputDialog(const gui::InputDialogParams &params,
	QString &ret_val)
{
	QDialog dialog(this);
	dialog.setWindowTitle(params.title);
	dialog.setModal(true);
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	dialog.setLayout(vert_layout);
	
	{ // icon + mime row
		QBoxLayout *row = new QBoxLayout(QBoxLayout::LeftToRight);
		
		if (params.icon != nullptr) {
			QLabel *icon_label = new QLabel();
			QPixmap pixmap = params.icon->pixmap(QSize(48, 48));
			icon_label->setPixmap(pixmap);
			row->addWidget(icon_label);
		}
		
		QLabel *text_label = new QLabel();
		text_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		text_label->setText(params.msg);
		row->addWidget(text_label, 2);
		
		vert_layout->addLayout(row);
	}
	
	QLineEdit *le = new QLineEdit();
	{ // file name row
		vert_layout->addWidget(le);
		le->setText(params.initial_value);
		le->setPlaceholderText(params.placeholder_text);
		if (params.selection_start != -1)
			le->setSelection(params.selection_start, params.selection_end);
	}
	
	{ // ok + cancel buttons row
		QDialogButtonBox *button_box = new QDialogButtonBox
			(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		vert_layout->addWidget(button_box);
	}
	
	dialog.resize(params.size);
	bool ok = dialog.exec();
	ret_val = le->text();
	return ok;
}

gui::Tab* App::tab() const
{
	return (gui::Tab*) tab_widget_->currentWidget();
}

void App::TabSelected(const int index)
{
	if (location_ == nullptr) {
		return;
	}
	gui::Tab *tab = (gui::Tab*)tab_widget_->widget(index);
	const QString &path = tab->current_dir();
	location_->SetLocation(path);
	tree_view_->MarkCurrentPartition(path);
	toolbar_->SyncViewModeWithCurrentTab();
	const QString s = tab->title() + QString(" — Cornus");
	setWindowTitle(s);
}

void App::TellUser(const QString &msg, const QString title) {
	QMessageBox::warning(this, title, msg);
}

void App::TestExecBuf(const char *buf, const isize size, ExecInfo &ret)
{
	if (size < 4)
		return;
	
	if (buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') {
		ret.type |= ExecType::Elf;
		return;
	}
	
	QString s = QString::fromLocal8Bit(buf, size);
	if (s.startsWith("#!")) {
		ret.type |= ExecType::ShellScript;
		
		int new_line = s.indexOf('\n');
		if (new_line == -1) {
			return;
		}
		
		const int start = 2;
		const int count = new_line - start;
		if (count > 0) {
			QStringRef starter = s.midRef(start, new_line - start);
			
			if (!starter.isEmpty())
				ret.starter = starter.trimmed().toString();
		}
		return;
	}
}

void App::ToggleExecBitOfSelectedFiles()
{
	auto &view_files = *files(tab()->files_id());
	MutexGuard guard = view_files.guard();
	const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;
	for (io::File *next: view_files.data.vec) {
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

void App::ViewChanged()
{
	mtl_tbd();
}

}
