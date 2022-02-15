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
#include "Hid.hpp"
#include "History.hpp"
#include "io/disks.hh"
#include "io/File.hpp"
#include "io/Files.hpp"
#include "io/io.hh"
#include "io/SaveFile.hpp"
#include "io/socket.hh"
#include "gui/actions.hxx"
#include "gui/ConfirmDialog.hpp"
#include "gui/IconView.hpp"
#include "gui/Location.hpp"
#include "gui/SearchPane.hpp"
#include "gui/sidepane.hh"
#include "gui/Tab.hpp"
#include "gui/TabBar.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "gui/TabsWidget.hpp"
#include "gui/TextEdit.hpp"
#include "gui/ToolBar.hpp"
#include "gui/TreeItem.hpp"
#include "gui/TreeModel.hpp"
#include "gui/TreeView.hpp"
#include "Media.hpp"
#include "prefs.hh"
#include "Prefs.hpp"
#include "str.hxx"
#include "thumbnail.hh"

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
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>

#include <sched.h>
#include <glib.h>
#include <polkit/polkit.h>
//#include <polkitagent/polkitagent.h>

namespace cornus {

void* ThumbnailLoader (void *args)
{
	pthread_detach(pthread_self());
	cornus::ThumbLoaderData *th_data = (cornus::ThumbLoaderData*) args;
	MTL_CHECK_ARG(th_data->Lock(), NULL);
	ZSTD_DCtx *decompress_context = ZSTD_createDCtx();
	ByteArray temp_ba;
	io::ReadParams read_params = {};
	read_params.print_errors = PrintErrors::No;
	read_params.can_rely = CanRelyOnStatxSize::Yes;
	
	while (th_data->wait_for_work)
	{
		if (th_data->new_work == nullptr)
		{
			const int status = th_data->CondWaitForNewWork();
			if (status != 0)
			{
				mtl_status(status);
				break;
			}
		}
		
		ThumbLoaderArgs *new_work = th_data->new_work;
		if (new_work == nullptr)
			continue;
		
		th_data->Unlock();
		QString thumb_in_temp_path = io::BuildTempPathFromID(new_work->file_id);
		Thumbnail *thumbnail = nullptr;
		const bool has_ext_attr = !new_work->ba.is_empty();
		temp_ba.to(0);
		//QString th_str = io::thread_id_short(pthread_self());
		//mtl_info("%s: %s", qPrintable(th_str), qPrintable(new_work->full_path));
		thumbnail::AbiType abi_version = -1;
		if (has_ext_attr || io::ReadFile(thumb_in_temp_path, temp_ba, read_params))
		{
			ByteArray &img_ba = has_ext_attr ? new_work->ba : temp_ba;
			i32 orig_img_w, orig_img_h;
			QImage img = thumbnail::ImageFromByteArray(img_ba,
				orig_img_w, orig_img_h, abi_version, decompress_context);
			if (!img.isNull())
			{
				thumbnail = new Thumbnail();
				thumbnail->abi_version = abi_version;
				thumbnail->img = img;
				thumbnail->file_id = new_work->file_id.inode_number;
				thumbnail->time_generated = time(NULL);
				thumbnail->w = img.width();
				thumbnail->h = img.height();
				thumbnail->original_image_w = orig_img_w;
				thumbnail->original_image_h = orig_img_h;
				thumbnail->tab_id = new_work->tab_id;
				thumbnail->dir_id = new_work->dir_id;
				thumbnail->origin = has_ext_attr ? Origin::ExtAttr : Origin::TempDir;
			}
		}
		if (thumbnail == nullptr)
		{
			thumbnail = thumbnail::Load(new_work->full_path,
				new_work->file_id.inode_number,
				new_work->ext, new_work->icon_w,
				new_work->icon_h, new_work->tab_id,
				new_work->dir_id);
			if (thumbnail)
				thumbnail->origin = Origin::DiskFile;
		}
		
		th_data->Lock();
		
		if (thumbnail == nullptr)
		{
			th_data->new_work = nullptr;
			//mtl_info("thumbnail failed %s", qPrintable(new_work->full_path));
			continue;
		}
		
		if (!th_data->wait_for_work)
		{
			delete thumbnail;
			break;
		}
		
		App *app = new_work->app;
		delete new_work;
		th_data->new_work = nullptr;
		if (th_data->wait_for_work)
		{
			QMetaObject::invokeMethod(app, "ThumbnailArrived",
				Q_ARG(cornus::Thumbnail*, thumbnail));
		} else {
			delete thumbnail;
			break;
		}
		
		{ // signal that the thread is waiting for new work
			th_data->Unlock();
			if (th_data->global_data->TryLock())
			{
				th_data->global_data->SignalWorkQueueChanged();
				th_data->global_data->Unlock();
			}
			th_data->Lock();
		}
	}
	
	ZSTD_freeDCtx(decompress_context);
	
	const u64 t = static_cast<u64>(pthread_self());
	th_data->thread_exited = true;
	// after Unlock() th_data might be already deleted, so save a pointer:
	auto *global_data = th_data->global_data;
	th_data->Unlock();
	
	// signal that the thread exited
	const bool unlock = global_data->Lock();
	global_data->Broadcast();
	if (unlock)
		global_data->Unlock();
	if (DebugThumbnailExit)
		mtl_trace("%ld EXIT", t);
	
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

void TestPolkit(App *app)
{
	//PolkitIdentity *identity = polkit_unix_user_new (getuid());
	//PolkitAgentSession *session = polkit_agent_session_new(identity);
}

void Print(const double x, const double y)
{
	const double rem = std::fmod(x, y);
	mtl_info("%.3f MOD %.3f = %f", x, y, rem);
}

App::App()
{
	/* const double y = 5.0;
	Print(6.2, y);
	Print(1.0, y);
	Print(11.0, y);
	Print(5.1, y);
	Print(4.9, y); */
	/*
	ElapsedTimer t;
	
	t.Continue();
	auto env = QProcessEnvironment::systemEnvironment();
	auto n1 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	env = QProcessEnvironment::systemEnvironment();
	auto n2 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	env = QProcessEnvironment::systemEnvironment();
	auto n3 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	env = QProcessEnvironment::systemEnvironment();
	auto n4 = t.elapsed_mc();
	
	mtl_info("%ld %ld %ld, %ld", n1, n2, n3, n4);
	
	t.Continue(Reset::Yes);
	auto env1 = env;
	auto c1 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	auto env2 = env;
	auto c2 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	auto env3 = env;
	auto c3 = t.elapsed_mc();
	
	t.Continue(Reset::Yes);
	auto env4 = env;
	auto c4 = t.elapsed_mc();
	
	mtl_info("%ld %ld %ld, %ld", c1, c2, c3, c4);
	*/
	
	Init();
}

App::~App()
{
	ShutdownThumbnailThreads();
	prefs_->Save();
	
	QHashIterator<QString, QIcon*> i(icon_set_);
	while (i.hasNext())
	{
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
	
	delete hid_;
	hid_ = nullptr;
	
	if (compress_ctx_)
	{
		ZSTD_freeCCtx(compress_ctx_);
		compress_ctx_ = nullptr;
	}

	for (auto *shortcut: shortcuts_)
		delete shortcut;
	shortcuts_.clear();
}

void App::ApplyDefaultPrefs()
{
	// first time, apply sane default prefs:
	prefs_->sync_views_scroll_location(true);
	prefs_->remember_window_size(true);
	prefs_->show_free_partition_space(true);
	prefs_->show_dir_file_count(true);
	prefs_->show_link_targets(true);
	prefs_->store_thumbnails_in_ext_attrs(true);
	
	auto &cols = prefs_->cols_visibility();
	cols[(int)gui::Column::Size] = 1;
	cols[(int)gui::Column::TimeModified] = 1;
	cols[(int)gui::Column::TimeCreated] = 0;
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
	tab()->view_files().GetSelectedFileNames(Lock::Yes, urls, Path::Full);
	if (urls.isEmpty())
		return;
	
	QString files_dir = tab()->current_dir();
	if (!files_dir.endsWith('/'))
		files_dir.append('/');
	
	for (int i = 0; i < urls.size(); i++)
	{
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
	int w = fm.horizontalAdvance(text);
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
	tab()->view_files().SelectFilenamesLater({file->name()}, SameDir::Yes);
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

int App::AvailableCpuCores() const
{
	// Caching available cores because according to the documentation
	// each sysconf() call opens and parses files in /sys. It's unclear
	// whether QThread::idealThreadCount() caches the result.
	static int available_cores_ = -1;
	
	if (available_cores_ == -1)
		available_cores_ = sysconf(_SC_NPROCESSORS_ONLN);
	
	return available_cores_;
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
	
	QSet<int> indices;
	tab()->table()->SyncWith(clipboard_, indices);
	tab()->table_model()->UpdateIndices(indices);
}

void App::CloseCurrentTab()
{
	CloseTabAt(tab_widget_->currentIndex());
}

void App::CloseTabAt(const int i)
{
	if (tab_widget_->count() == 1)
	{
		QApplication::quit();
	} else {
		auto *tab = tab_widget_->widget(i);
		tab_widget_->removeTab(i);
		delete tab;
	}
}

void App::CreateGui()
{
	toolbar_ = new gui::ToolBar(this);
	location_ = toolbar_->location();
	addToolBar(toolbar_);
	
	top_level_stack_.stack = new QStackedWidget();
	setCentralWidget(top_level_stack_.stack);
	
	main_splitter_ = new QSplitter(Qt::Horizontal);
	top_level_stack_.window_index = top_level_stack_.stack->addWidget(main_splitter_);
	top_level_stack_.stack->setCurrentIndex(top_level_stack_.window_index);
	
	CreateSidePane();
	CreateFilesViewPane();
	
	main_splitter_->setStretchFactor(0, 0);
	main_splitter_->setStretchFactor(1, 1);
	auto &sizes = prefs_->splitter_sizes();
	if (sizes.size() > 0)
		main_splitter_->setSizes(sizes);

	prefs_->UpdateTableSizes();
	
	{
		QIcon icon = QIcon::fromTheme(QLatin1String("window-new"));
		QToolButton *btn = new QToolButton();
		btn->setIcon(icon);
		connect(btn, &QAbstractButton::clicked, [=] () {
			OpenNewTab();
		});
		tab_widget_->setCornerWidget(btn, Qt::TopRightCorner);
	}
	
	tab()->FocusView();
}

void App::CreateFilesViewPane()
{
	QWidget *table_pane = new QWidget();
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	vert_layout->setContentsMargins(0, 0, 0, 0);
	vert_layout->setSpacing(0);
	table_pane->setLayout(vert_layout);
	
	vert_layout->addWidget(tab_widget_);
	
	search_pane_ = new gui::SearchPane(this);
	vert_layout->addWidget(search_pane_);
	search_pane_->setVisible(false);
	
	main_splitter_->addWidget(table_pane);
	{
		auto g = gui_bits_.guard();
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
			tab()->view_files().SelectFilenamesLater({name}, SameDir::Yes);
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
	{
		auto guard = tree_data_.guard();
		tree_data_.widgets_created = true;
		tree_data_.Broadcast();
	}
}

DirId App::current_dir_id() const
{
	auto &files = tab()->view_files();
	auto g = files.guard();
	return files.data.dir_id;
}

void App::DeleteFilesById(const FilesId id)
{
	io::Files *p = files_.value(id, nullptr);
	if (p != nullptr)
	{
		files_.remove(id);
		delete p;
	}
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
	if (cloned_file == nullptr)
	{
		cloned_file = tab()->view_files().GetFileAtIndex(Lock::Yes, row);
		if (cloned_file == nullptr)
			return;
	}
	
	SetTopLevel(TopLevel::Editor, cloned_file);
}

void App::DisplaySymlinkInfo(io::File &file)
{
	if (!file.has_link_target())
		file.ReadLinkTarget();
	
	io::LinkTarget *t = file.link_target();
	MTL_CHECK_VOID(t != nullptr);
	
	QDialog dialog(this);
	dialog.setWindowTitle("Symlink path chain");
	QFormLayout *layout = new QFormLayout();
	dialog.setLayout(layout);
	QFont font;
	QFontMetricsF font_metrics(font);
	int max_len = 20;
	
	for (const QString &next: t->chain_paths_)
	{
		const auto w = font_metrics.horizontalAdvance(next);
		if (w > max_len)
			max_len = w + 50;
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
	QString full_path = tab()->view_files().GetFirstSelectedFileFullPath(Lock::Yes, &ext);
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
	tab()->GetSelectedArchives(urls);
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

void App::FileDoubleClicked(io::File *file, const PickBy pb)
{
	cornus::AutoDelete ad(file);

	if (pb == PickBy::Icon)
	{
		if (file->is_symlink()) {
			DisplaySymlinkInfo(*file);
		} else {
			DisplayFileContents(-1, file->Clone());
		}
	} else if (pb == PickBy::VisibleName) {
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

io::Files* App::files(const FilesId id) const
{
	return files_.value(id, nullptr);
}

FilesId App::GenNextFilesId()
{
	next_files_id_++;
	files_.insert(next_files_id_, new io::Files());
	return next_files_id_;
}

QIcon* App::GetDefaultIcon()
{ // this function must not return nullptr
	if (icon_cache_.unknown == nullptr)
	{
		QString full_path = GetIconThatStartsWith(QLatin1String("text"));
		if (full_path.isEmpty())
		{
			const QIcon ic = QIcon::fromTheme(QLatin1String("text-x-generic"));
			icon_cache_.unknown = new QIcon(ic);
		} else {
			icon_cache_.unknown = GetIconOrLoadExisting(full_path);
		}
	}
	return icon_cache_.unknown;
}

QIcon* App::GetFileIcon(io::File *file)
{
	if (file->cache().icon == nullptr)
		file->cache().icon = LoadIcon(*file);
	
	return file->cache().icon;
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

QColor App::hover_bg_color_gray(const QColor &c) const
{
	if (theme_type_ == ThemeType::Dark)
		return QColor(90, 90, 90);
	
	const QColor lght = c.lighter(180);
	const int avg = (lght.red() + lght.green() + lght.blue()) / 3;
	
	return (avg >= 240) ? c.lighter(140) : lght;
}

Range get_policy_range(const int policy)
{
	const int min = sched_get_priority_min(policy);
	const int max = sched_get_priority_max(policy);
	if (min == -1 || max == -1) {
		mtl_status(errno);
		return Range::Invalid();
	}
	
	mtl_info("Policy: %d, min: %d, max: %d", policy, min, max);
	
	return Range{.min = min, .max = max};
}

void SetThreadPolicyAndPriority(const pthread_t &th, const int policy,
	const int priority)
{
	pthread_attr_t th_attr;
	sched_param th_param = {};
	int status = pthread_attr_init (&th_attr);
	if (status != 0)
	{
		mtl_status(status);
		return;
	}
	
	status = pthread_attr_setschedpolicy(&th_attr, policy);
	if (status != 0)
	{
		mtl_status(status);
		return;
	}
	status = pthread_attr_getschedparam (&th_attr, &th_param);
	if (status != 0)
	{
		mtl_status(status);
		return;
	}
	th_param.sched_priority = priority;
	
	status = pthread_setschedparam(th, policy, &th_param);
	if (status != 0)
	{
		mtl_status(status);
	}
}

void App::Init()
{
	qRegisterMetaType<cornus::io::File*>();
	qRegisterMetaType<cornus::io::FilesData*>();
	qRegisterMetaType<cornus::io::FileEvent>();
	qRegisterMetaType<cornus::PartitionEvent*>();
	qRegisterMetaType<cornus::io::CountRecursiveInfo*>();
	qRegisterMetaType<QVector<cornus::gui::TreeItem*>>();
	qDBusRegisterMetaType<QMap<QString, QVariant>>();
	qRegisterMetaType<cornus::Thumbnail*>();
	locale_ = QLocale::system();
	media_ = new Media();
	hid_ = new Hid(this);
	
	io::NewThread(gui::sidepane::LoadItems, this);
	io::socket::AutoLoadRegularIODaemon();
	setWindowIcon(QIcon(cornus::AppIconPath));
	prefs_ = new Prefs(this);
	if (!prefs_->Load())
		ApplyDefaultPrefs();
	
	{
		tab_widget_ = new gui::TabsWidget();
		tab_bar_ = new gui::TabBar(this);
		tab_widget_->SetTabBar(tab_bar_);
		connect(tab_widget_, &QTabWidget::tabCloseRequested, this, &App::CloseTabAt);
		connect(tab_widget_, &QTabWidget::currentChanged, this, &App::TabSelected);
		OpenNewTab(FirstTime::Yes);
	}
	
	env_ = QProcessEnvironment::systemEnvironment();
	io::InitEnvInfo(desktop_, search_icons_dirs_, xdg_data_dirs_,
		possible_categories_, env_);
	SetupIconNames();
	CreateGui();
	if (prefs_->remember_window_size())
	{
		QSize sz = prefs_->window_size();
		if (sz.width() > 5 || sz.height() > 5)
		{
			resize(sz);
		}
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
	
//	TestPolkit(this);
	//thumbnail::LoadWebpImage("/home/fox/scale_1200.webp", 128, 128);
	
}

void App::InitThumbnailPoolIfNeeded()
{
	if (!global_thumb_loader_data_.threads.isEmpty())
		return;
	
	int status;
	if (false)
	{
		static bool first_time = true;
		if (first_time)
		{
			first_time = false;
			SetThreadPolicyAndPriority(pthread_self(), SCHED_RR, 2);
		}
		
		pthread_attr_t th_attr;
		status = pthread_attr_init (&th_attr);
		if (status != 0)
		{
			mtl_status(status);
			return;
		}
		
		int current_policy;
		status = pthread_attr_getschedpolicy(&th_attr, &current_policy);
		if (status != 0)
		{
			mtl_status(status);
			return;
		}
		
		mtl_info("Current policy: %d", current_policy);
		const Range range = get_policy_range(current_policy);
		MTL_CHECK_VOID(range.is_valid());
		
		mtl_info("Policy SCHED_OTHER: %d", SCHED_OTHER);
		get_policy_range(SCHED_OTHER);
		
		mtl_info("Policy SCHED_FIFO: %d", SCHED_FIFO);
		get_policy_range(SCHED_FIFO);
		
		mtl_info("Policy SCHED_RR: %d", SCHED_RR);
		get_policy_range(SCHED_RR);
	}
	
	// leave a thread for other background tasks
	const int max_thread_count = std::max(1, AvailableCpuCores()/* - 1*/);
	for (int i = 0; i < max_thread_count; i++)
	{
		ThumbLoaderData *thread_data = new ThumbLoaderData();
		thread_data->global_data = &global_thumb_loader_data_;
		if (!io::NewThread(ThumbnailLoader, thread_data))
		{
			delete thread_data;
			return;
		}
		
		global_thumb_loader_data_.threads.append(thread_data);
	}
	
	io::NewThread(thumbnail::LoadMonitor, &global_thumb_loader_data_);
	
//	status = pthread_attr_destroy(&th_attr);
//	if (status != 0)
//		mtl_status(status);
}

void App::LaunchOrOpenDesktopFile(const QString &full_path,
	const bool has_exec_bit, const RunAction action)
{
	bool open = action == RunAction::Open;
	if (!open)
		open = (action == RunAction::ChooseBasedOnExecBit) && !has_exec_bit;
	
	if (open)
	{
		OpenWithDefaultApp(full_path);
		return;
	}
	
	DesktopFile *df = DesktopFile::FromPath(full_path, possible_categories_, env_);
	if (df == nullptr)
		return;
	DesktopArgs args;
	args.working_dir = tab()->current_dir();
	df->Launch(args);
	delete df;
}

QIcon* App::LoadIcon(io::File &file)
{
	if (file.is_dir_or_so())
	{
		if (icon_cache_.folder == nullptr)
		{
			QString full_path = GetIconThatStartsWith(QLatin1String("special_folder"));
			MTL_CHECK_ARG(!full_path.isEmpty(), nullptr);
			icon_cache_.folder = GetIconOrLoadExisting(full_path);
		}
		
		return icon_cache_.folder;
	}
	
	const auto &fn = file.name_lower();
	const auto &ext = file.cache().ext;
	
	if (ext.isEmpty())
	{
		
		if (fn.indexOf(QLatin1String(".so.")) != -1)
		{
			if (icon_cache_.lib == nullptr) {
				QString full_path = GetIconThatStartsWith(QLatin1String("special_sharedlib"));
				MTL_CHECK_ARG(!full_path.isEmpty(), nullptr);
				icon_cache_.lib = GetIconOrLoadExisting(full_path);
			}
			return icon_cache_.lib;
		}
		
		return GetDefaultIcon();
	}
	
	static const QString desktop_ext = QLatin1String("desktop");
	if (ext == desktop_ext)
	{
		if (file.cache().desktop_file != nullptr)
		{
			QIcon icon = file.cache().desktop_file->CreateQIcon();
			// just testing icon.isNull() doesn't test if the image
			// can be loaded, so:
			if (!icon.pixmap(QSize(32, 32)).isNull())
			{
				return new QIcon(icon);
			}
		}
	}
	
	QString filename = icon_names_.value(ext.toString());
	
	if (!filename.isEmpty()) {
		return GetIconOrLoadExisting(filename);
	}
	
	if (ext.startsWith(QLatin1String("blend"))) {
		QString full_path = GetIconThatStartsWith(QLatin1String("special_blender"));
		MTL_CHECK_ARG(full_path.size() > 0, nullptr);
		return GetIconOrLoadExisting(full_path);
	}
	
	return GetDefaultIcon();
}

void App::LoadIconsFrom(QString dir_path)
{
	QVector<QString> available_names;
	if (!io::ListFileNames(dir_path, available_names))
		return;
	
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
	static TabId tab_id = 0;
	gui::Tab *tab = new gui::Tab(this, ++tab_id);
	tab_widget_->addTab(tab, QString());
	if (ft == FirstTime::Yes) {
		tab->GoToInitialDir();
	} else {
		QString dir_path = this->tab()->current_dir();
		tab->GoToSimple(dir_path);
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

void App::OpenWithDefaultApp(const QString &full_path)
{
	ByteArray ba;
	ba.set_msg_id(io::Message::SendDefaultDesktopFileForFullPath);
	ba.add_string(full_path);
	int fd = io::socket::Client();
	MTL_CHECK_VOID(fd != -1);
	MTL_CHECK_VOID(ba.Send(fd, CloseSocket::No));
	ba.Clear();
	MTL_CHECK_VOID(ba.Receive(fd));
	
	if (!io::CheckDesktopFileABI(ba))
	{
		TellUserDesktopFileABIDoesntMatch();
		return;
	}
	
	DesktopFile *p = DesktopFile::From(ba, env_);
	MTL_CHECK_VOID(p != nullptr);
	DesktopArgs args;
	args.full_path = full_path;
	args.working_dir = tab()->current_dir();
	p->Launch(args);
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

QShortcut* App::Register(const QKeySequence ks)
{
	auto *sp = new QShortcut(ks, this);
	sp->setContext(Qt::ApplicationShortcut);
	shortcuts_.append(sp);
	return sp;
}

void App::RegisterShortcuts()
{
	QShortcut *sp;
	{
		sp = Register(QKeySequence(Qt::ALT + Qt::Key_Up));
		connect(sp, &QShortcut::activated, this, &App::GoUp);
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_U));
		connect(sp, &QShortcut::activated, [=] {
//			tab()->UndoDelete(0); // 0 = most recent batch, -1 = all
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_O));
		connect(sp, &QShortcut::activated, [=] { OpenTerminal(); });
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_0));
		connect(sp, &QShortcut::activated, [=] {
			prefs_->WheelEventFromMainView(Zoom::Reset);
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_H));
		connect(sp, &QShortcut::activated, [=] {
			prefs_->show_hidden_files(!prefs_->show_hidden_files());
			tab()->GoTo(Action::Reload, {tab()->current_dir(), Processed::Yes}, Reload::Yes);
			prefs_->Save();
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_F));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				search_pane_->SetSearchByFileName();
				search_pane_->setVisible(true);
				search_pane_->RequestFocus();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_M));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				search_pane_->SetSearchByMediaXattr();
				search_pane_->setVisible(true);
				search_pane_->RequestFocus();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_W));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				CloseCurrentTab();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_Q));
		connect(sp, &QShortcut::activated, [=] {
			QApplication::quit();
		});
	}
	{
		sp = Register(QKeySequence(Qt::SHIFT + Qt::Key_Delete));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				tab()->DeleteSelectedFiles(ShiftPressed::Yes);
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_I));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				tab()->FocusView();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_L));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				location_->setFocus();
				location_->selectAll();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_A));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				gui::Tab *tab = this->tab();
				tab->FocusView();
				QSet<int> indices;
				auto &view_files = tab->view_files();
				MutexGuard guard = view_files.guard();
				view_files.SelectAllFiles(Lock::No, Selected::Yes, indices);
				tab->table_model()->UpdateIndices(indices);
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_E));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				ToggleExecBitOfSelectedFiles();
			}
		});
	}
	{
		sp = Register(QKeySequence(Qt::CTRL + Qt::Key_T));
		connect(sp, &QShortcut::activated, [=] {
			if (level_browser())
			{
				OpenNewTab();
			}
		});
	}
}

void App::RegisterVolumesListener()
{
	io::NewThread(gui::sidepane::monitor_devices, this);
	GVolumeMonitor *monitor = g_volume_monitor_get ();
	g_signal_connect(monitor, "mount-added", (GCallback)MountAdded, this);
	g_signal_connect(monitor, "mount-removed", (GCallback)MountRemoved, this);
	//g_signal_connect(monitor, "mount-changed", (GCallback)MountChanged, this);
}

void App::Reload()
{
	gui::Tab *tab = this->tab();
	QVector<QString> filenames;
	tab->view_files().GetSelectedFileNames(Lock::Yes, filenames, Path::OnlyName);
	const int vscroll = tab->GetScrollValue();
	tab->view_files().SelectFilenamesLater(filenames, SameDir::No);
	tab->GoTo(Action::Reload, {tab->current_dir(), Processed::Yes}, Reload::Yes);
	tab->SetScrollValue(vscroll);
}

void App::RemoveAllThumbTasksExcept(const DirId dir_id)
{
	auto global_guard = global_thumb_loader_data_.guard();
	auto &work_queue = global_thumb_loader_data_.work_queue;
	const int count = work_queue.size();
	int removed_count = 0;
	for (int i = count - 1; i >= 0; i--)
	{
		ThumbLoaderArgs *arg = work_queue[i];
		if (arg->dir_id != dir_id)
		{
			removed_count++;
			delete arg;
			work_queue.remove(i);
		}
	}
	
	if (removed_count > 0) {
		mtl_info("Removed work items: %d", removed_count);
		global_thumb_loader_data_.Broadcast();
	}
}

void App::RenameSelectedFile()
{
	io::File *file = nullptr;
	gui::Tab *tab = this->tab();
	auto &files = tab->view_files();
	if (files.GetFirstSelectedFile(Lock::Yes, &file, Clone::Yes) == -1)
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
	int w = fm.horizontalAdvance(text);
	w = std::min(800, std::max(w + 80, 400)); // between 300 and 800
	dialog.resize(w, 100);
	bool ok = dialog.exec();
	if (!ok)
		return;
	
	QString value = le->text();
	if (value.isEmpty())
		return;
	
	auto old_path = file->build_full_path().toLocal8Bit();
	auto new_path = (file->dir_path() + value).toLocal8Bit();
	
	if (old_path == new_path)
		return;
	
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(file->dir_path(), needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = WaitForRootDaemon(CanOverwrite::No);
		MTL_CHECK_VOID(hash_info.valid());
		
		auto *ba = new ByteArray();
		ba->add_u64(hash_info.num);
		const auto msg =  io::Message::RenameFile;
		ba->set_msg_id(msg);
		ba->add_string(old_path);
		ba->add_string(new_path);
		io::socket::SendAsync(ba, socket_path);
	} else if (::rename(old_path.data(), new_path.data()) != 0) {
		QString err = QString("Failed: ") + strerror(errno);
		QMessageBox::warning(this, "Failed", err);
	}
	
	tab->view_files().SelectFilenamesLater({value});
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
	MTL_CHECK_VOID(bkm_root != nullptr);
	for (gui::TreeItem *next: bkm_root->children())
	{
		item_vec.append(next->Clone());
	}
	
	io::SaveFile save_file(prefs::GetBookmarksFilePath());
	
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
	
	MTL_CHECK_VOID(io::WriteToFile(save_file.GetPathToWorkWith(), buf.data(), buf.size()));
	save_file.Commit();
}

void App::SaveThumbnail()
{
	if (compress_ctx_ == nullptr)
		compress_ctx_ = ZSTD_createCCtx();
	
	if (!thumbnails_to_save_.isEmpty())
		io::SaveThumbnailToDisk(thumbnails_to_save_.takeLast(), compress_ctx_,
			prefs_->store_thumbnails_in_ext_attrs());
}

void App::SelectCurrentTab()
{
	TabSelected(tab_widget_->currentIndex());
}

void App::SelectTabAt(const int tab_index, const FocusView fv)
{
	const int count = tab_widget_->count();
	if (count == 0 || tab_index < 0 || tab_index >= count)
		return;
	
	if (tab_index != tab_widget_->currentIndex())
	{
		tab_widget_->setCurrentIndex(tab_index);
	}
	
	if (fv == FocusView::Yes)
	{
		tab()->FocusView();
	}
}

void App::SetTopLevel(const TopLevel tl, io::File *cloned_file)
{
	if (top_level_stack_.level == tl)
		return;
	
	if (tl == TopLevel::Browser)
	{
		top_level_stack_.level = tl;
		if (top_level_stack_.stack->currentIndex() == top_level_stack_.window_index)
			return;
		
		setWindowTitle(top_level_stack_.saved_window_title);
		toolbar_->setVisible(true);
		top_level_stack_.stack->setCurrentIndex(top_level_stack_.window_index);
	} else if (tl == TopLevel::Editor) {
		if (top_level_stack_.editor == nullptr)
		{
			top_level_stack_.editor = new gui::TextEdit(tab());
			top_level_stack_.editor_index = top_level_stack_.stack->addWidget(top_level_stack_.editor);
		}
		
		top_level_stack_.saved_window_title = windowTitle();
		if (top_level_stack_.editor->Display(cloned_file))
		{
			setWindowTitle(tr("Esc => Exit, Ctrl+S => Save & Exit"));
			toolbar_->setVisible(false);
			top_level_stack_.stack->setCurrentIndex(top_level_stack_.editor_index);
			tab()->table()->ClearMouseOver();
			top_level_stack_.level = tl;
		}
	}
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

void App::showEvent(QShowEvent *evt)
{
	//mtl_info("spontaneous: %d", evt->spontaneous());
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

void App::ShutdownThumbnailThreads()
{
	global_thumb_loader_data_.Lock();
	int global_index = 0;
	for (ThumbLoaderData *th_data: global_thumb_loader_data_.threads)
	{
		if (DebugThumbnailExit)
			mtl_info("Terminating thread %d", global_index++);
		// Terminate thumb loading threads if any
		th_data->Lock();
		th_data->wait_for_work = false;
		th_data->SignalNewWorkAvailable();
		th_data->Unlock();
	}
	
	struct timespec till;
	const i64 ms = 50 * 1000 * 1000;
	auto &threads_vec = global_thumb_loader_data_.threads;
	while (!threads_vec.isEmpty())
	{
		for (int i = threads_vec.size() - 1; i >= 0; i--)
		{
			ThumbLoaderData *item = threads_vec[i];
			item->Lock();
			const bool exited = item->thread_exited;
			item->Unlock();
			if (exited)
			{
				if (DebugThumbnailExit)
					mtl_info("Thread %d exited", i);
				delete item;
				threads_vec.remove(i);
			}
		}
		
		if (DebugThumbnailExit)
			mtl_trace();
		
		clock_gettime(CLOCK_MONOTONIC, &till);
		till.tv_nsec += ms;
		// wait for threads termination
		global_thumb_loader_data_.CondTimedWait(&till);
		if (DebugThumbnailExit)
			mtl_trace();
	}
	global_thumb_loader_data_.Unlock();
}

void App::SubmitThumbLoaderBatchFromTab(QVector<ThumbLoaderArgs*> *new_work_vec,
	const TabId tab_id, const DirId dir_id)
{
	if (new_work_vec->isEmpty())
	{
		delete new_work_vec;
		return;
	}
	
	{
		auto global_guard = global_thumb_loader_data_.guard();
		InitThumbnailPoolIfNeeded();
		auto &work_queue = global_thumb_loader_data_.work_queue;
		const int count = work_queue.size();
		int removed_count = 0;
		for (int i = count - 1; i >= 0; i--)
		{
			ThumbLoaderArgs *arg = work_queue[i];
			if (arg->tab_id != tab_id || arg->dir_id != dir_id)
			{
				removed_count++;
				delete arg;
				work_queue.remove(i);
			}
		}
		
		for (int i = new_work_vec->size() - 1; i >= 0; i--)
		{
// Iteration happens from last to first on purpose because the user needs
// the thumbnails to be loaded from top to bottom and since the thumbnails
// monitor picks from the bottom of the vector queue with vector->takeLast()
// (because it's more efficient than vector->takeFirst()) one must submit
// the items from last to first.
			auto *item = (*new_work_vec)[i];
			work_queue.append(item);
		}
		
		global_thumb_loader_data_.Broadcast();
	}
	
	delete new_work_vec;
}

gui::Tab* App::tab() const
{
	return (gui::Tab*) tab_widget_->currentWidget();
}

gui::Tab* App::tab(const TabId id, int *ret_index)
{
	const int count = tab_widget_->count();
	for (int i = 0; i < count; i++)
	{
		auto *next = (gui::Tab*) tab_widget_->widget(i);
		if (next->id() == id)
		{
			if (ret_index)
				*ret_index = i;
			return next;
		}
	}
	
	return nullptr;
}

gui::Tab* App::tab_at(const int tab_index) const
{
	return (gui::Tab*) tab_widget_->widget(tab_index);
}

void App::TabSelected(const int index)
{
	if (location_ == nullptr || index == -1)
	{
		// index can be -1 when Ctrl+W the only existing tab.
		return;
	}
	gui::Tab *tab = (gui::Tab*)tab_widget_->widget(index);
	const QString &path = tab->current_dir();
	location_->SetLocation(path);
	tree_view_->MarkCurrentPartition(path);
	toolbar_->SyncViewModeWithCurrentTab();
	QString title_str = tab->title() + QString(" — Cornus");
	
	if (prefs().show_ms_files_loaded())
	{
		title_str.append(' ');
		title_str.append(tab->ListSpeedString());
	}
	
	setWindowTitle(title_str);
}

void App::TellUser(const QString &msg, const QString title)
{
	QMessageBox::warning(this, title, msg);
}

void App::TellUserDesktopFileABIDoesntMatch()
{
	TellUser(tr("Please restart IO daemon: desktop file ABI doesn't match."));
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

void App::ThumbnailArrived(cornus::Thumbnail *thumbnail)
{
	gui::Tab *tab = this->tab(thumbnail->tab_id);
	if (tab == nullptr || tab->icon_view() == nullptr)
	{
		delete thumbnail;
		return;
	}
	
	auto &files = tab->view_files();
	{
		MTL_CHECK_VOID(files.Lock());
		const DirId dir_id = files.data.dir_id;
		
		if (thumbnail->dir_id != dir_id)
		{
			files.Unlock();
			delete thumbnail;
			return;
		}
		
		const bool can_write_to_dir = io::CanWriteToDir(files.data.processed_dir_path);
		for (io::File *file: files.data.vec)
		{
			if (file->id_num() != thumbnail->file_id)
				continue;
			
			file->thumbnail(thumbnail);
			const io::DiskFileId file_id = file->id();
			QString full_path = file->build_full_path();
			files.Unlock();
			tab->icon_view()->RepaintLater();
			
			if (thumbnail->origin == Origin::DiskFile)
			{
				io::SaveThumbnail st;
				st.full_path = full_path;
				st.id = file_id;
				st.orig_img_w = thumbnail->original_image_w;
				st.orig_img_h = thumbnail->original_image_h;
				st.thmb = thumbnail->img;
				st.dir = can_write_to_dir ? TempDir::No : TempDir::Yes;
				thumbnails_to_save_.append(st);
				
				QTimer::singleShot(0, this, &App::SaveThumbnail);
			}
			return;
		}
		
		files.Unlock();
	}
	
	delete thumbnail;
}

void App::ToggleExecBitOfSelectedFiles()
{
	auto &view_files = *files(tab()->files_id());
	MutexGuard guard = view_files.guard();
	const auto ExecBits = S_IXUSR | S_IXGRP | S_IXOTH;
	for (io::File *next: view_files.data.vec)
	{
		if (next->is_selected()) {
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

HashInfo App::WaitForRootDaemon(const CanOverwrite co)
{
	QHash<IOAction, QString> options;
	if (co == CanOverwrite::Yes)
	{
		options.insert(IOAction::AutoRenameAll, tr("Auto rename"));
		options.insert(IOAction::SkipAll, tr("Skip"));
		options.insert(IOAction::OverwriteAll, tr("Overwrite"));
	}
	
	gui::ConfirmDialog dialog(this, options, IOAction::AutoRenameAll);
	dialog.setWindowTitle(tr("Password required"));
	if (co == CanOverwrite::Yes)
		dialog.SetComboLabel(tr("Existing files:"));
	dialog.SetMessage(tr("Root password required, continue?"));
	int status = dialog.exec();
	if (status != QDialog::Accepted)
		return {};
	
	const IOActionType io_action = static_cast<IOActionType>(dialog.combo_value().toInt());
	const u64 secret = QRandomGenerator::global()->generate64();
	const QByteArray secret_ba = QByteArray::number(qulonglong(secret));
	const QByteArray hash_ba = QCryptographicHash::hash(secret_ba, QCryptographicHash::Md5);
	const QString hash_str = QString(hash_ba.toHex());
	// mtl_info("hash(%d): %s of %lu", hash_str.size(), qPrintable(hash_str), secret);
	const char *socket_p = cornus::RootSocketPath;
	ByteArray check_alive_ba;
	check_alive_ba.add_u64(secret);
	check_alive_ba.set_msg_id(io::Message::CheckAlive);
	
	const QString daemon_dir_path = QCoreApplication::applicationDirPath();
	const QString app_to_execute = daemon_dir_path + QLatin1String("/cornus_r");
	
	if (true)
	{
		QProcess *process = new QProcess();
		process->setProgram(QLatin1String("pkexec"));
		QStringList args;
		args << app_to_execute;
		args << hash_str;
		args << QString::number(io_action);
		process->setArguments(args);
		process->start();
	}

	/*const int result = process->exitCode();
mtl_trace("%ld, result: %d", time(NULL), result);
const int DismissedAuthorization = 126;
	const int AuthorizationDeclined = 127;
	if (result == DismissedAuthorization || result == AuthorizationDeclined)
	{
//		pkexec: Upon successful completion, the return value is the
//		return value of PROGRAM. If the calling process is not authorized
//		or an authorization could not be obtained through authentication
//		or an error occured, pkexec exits with a return value of 127.
//		If the authorization could not be obtained because the user
//		dismissed the authentication dialog, pkexec exits with a
//		return value of 126.
		
mtl_trace("dismissed");
		return {};
	} */
	bool launched = false;
	// wait till daemon is started:
	for (int i = 0; i < 40; i++)
	{
		if (io::socket::SendSync(check_alive_ba, socket_p))
		{
			launched = true;
			root_hash_ = HashInfo {.num = secret, .hash_str = hash_str};
			break;
		}
		usleep(100 * 1000); // 100ms
	}
	
	return launched ? root_hash_ : HashInfo();
}

}
