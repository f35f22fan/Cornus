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

#include "App.hpp"
#include "AutoDelete.hh"
#include "io/io.hh"
#include "io/File.hpp"
#include "gui/Location.hpp"
#include "gui/SidePane.hpp"
#include "gui/SidePaneModel.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "gui/ToolBar.hpp"

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
#include <QSplitter>
#include <QUrl>

namespace cornus {

App::App(): app_icon_(QLatin1String(":/resources/cornus.png"))
 {
	SetupIconNames();
	SetupEnvSearchPaths();
	CreateGui();
	GoToInitialDir();
	RegisterShortcuts();
	setWindowIcon(app_icon_);
	resize(900, 600);
}

App::~App() {
	QMapIterator<QString, QIcon*> i(icon_set_);
	while (i.hasNext()) {
		i.next();
		QIcon *icon = i.value();
		delete icon;
	}
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

void App::CreateGui() {
	QSplitter *splitter = new QSplitter(Qt::Horizontal);
	setCentralWidget(splitter);
	
	side_pane_model_ = new gui::SidePaneModel(this);
	side_pane_ = new gui::SidePane(side_pane_model_);
	side_pane_model_->SetSidePane(side_pane_);
	splitter->addWidget(side_pane_);
	
	table_model_ = new gui::TableModel(this);
	table_ = new gui::Table(table_model_);
	splitter->addWidget(table_);
	
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 5);
	
	toolbar_ = new gui::ToolBar(this);
	location_ = toolbar_->location();
	addToolBar(toolbar_);
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
			GoTo(full_path);
		} else if (file->is_link_to_dir()) {
			if (file->link_target() != nullptr) {
				GoTo(file->link_target()->path);
			}
		} else if (file->is_regular()) {
			QUrl url = QUrl::fromLocalFile(file->build_full_path());
			QDesktopServices::openUrl(url);
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

bool App::GoTo(QString dir_path, bool reload)
{
	if (!reload) {
		if (table_model_->IsAt(dir_path))
			return false;
	}
	
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	const auto start_time = std::chrono::steady_clock::now();
	io::Files files;
	io::Files *tfiles = table_model_->files();
	{
		MutexGuard guard(&tfiles->mutex);
		files.sorting_order = tfiles->sorting_order;
	}
	files.show_hidden_files = prefs_.show_hidden_files;
	files.dir_path = dir_path;
	
	if (io::ListFiles(files) != io::Err::Ok) {
		return false;
	}
	
	int count = files.vec.size();
	table_model_->SwitchTo(files);
	current_dir_ = dir_path;
	const int row = 0;
	QModelIndex index = table_model_->index(row, 0, QModelIndex());
	table_->scrollTo(index);
	auto now = std::chrono::steady_clock::now();
	
	const float elapsed = std::chrono::duration<float,
		std::chrono::milliseconds::period>(now - start_time).count();
	QString diff_str = io::FloatToString(elapsed, 2);
	
	QString title = QDir(dir_path).dirName() + QLatin1String(" [") +
		QString::number(count) + QLatin1String(" files in ") +
		diff_str + QLatin1String(" ms]");
	location_->SetLocation(dir_path);
	setWindowTitle(title);
	
	return true;
}

void App::GoToInitialDir() {
	const QStringList args = QCoreApplication::arguments();
	
	if (args.size() <= 1) {
		GoTo(QDir::homePath());
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
	
	if (!cmds.go_to_path.isEmpty()) {
		io::FileType file_type;
		if (!io::FileExists(cmds.go_to_path, &file_type)) {
			QString msg = "Directory doesn't exist:\n\""
				+ cmds.go_to_path + QChar('\"');
			TellUser(msg);
			GoHome();
			return;
		}
		if (file_type == io::FileType::Dir) {
			GoTo(cmds.go_to_path);
		} else {
			// go to parent dir of cmds.go_to_path and select file cmds.go_to_path
			QDir parent_dir(cmds.go_to_path);
			if (!parent_dir.cdUp()) {
				QString msg = "Can't access parent dir of file:\n\"" +
					cmds.go_to_path + QChar('\"');
				TellUser(msg);
				GoHome();
				return;
			}
			GoTo(parent_dir.absolutePath());
			table_->ScrollToAndSelect(cmds.go_to_path);
			return;
		}
	}
	
	if (!cmds.select.isEmpty()) {
		QString select_path;
		if (cmds.go_to_path.isEmpty()) {
			/// cmds.go_to_path should be the parent dir of cmds.select
			if (!io::FileExists(cmds.select)) {
				QString msg = "File to select doesn't exist:\n\""
					+ cmds.select + QChar('\"');
				TellUser(msg);
				GoHome();
				return;
			}
			QDir parent_dir(cmds.select);
			if (!parent_dir.cdUp()) {
				TellUser("Can't access parent dir of file to select");
				GoHome();
				return;
			}
			GoTo(parent_dir.absolutePath());
			select_path = cmds.select;
		} else {
			/// select_path is relative to cmds.go_to_path
			QString dir_path = cmds.go_to_path;
			if (!dir_path.endsWith('/'))
				dir_path.append('/');
			select_path = dir_path + cmds.select;
			if (!io::FileExists(select_path)) {
				QString msg = "File to select doesn't exist:\n\""
					+ select_path + QChar('\"');
				TellUser(msg);
				return;
			}
		}
		
		if (!select_path.isEmpty())
			table_->ScrollToAndSelect(select_path);
	}
}

void App::GoHome() {
	GoTo(QDir::homePath());
}

void App::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	CHECK_TRUE_VOID(parent.exists());
	QString parent_dir = parent.absolutePath();
	bool same;
	CHECK_TRUE_VOID((io::SameFiles(parent_dir, current_dir_, same) == io::Err::Ok));
	
	if (!same) {
		CHECK_TRUE_VOID(GoTo(parent_dir));
	}
	
	table_->ScrollToAndSelect(full_path);
}

void App::GoUp() {
	if (current_dir_.isEmpty())
		return;
	
	QDir dir(current_dir_);
	
	if (!dir.cdUp())
		return;
	
	QString select_path = current_dir_;
	GoTo(dir.absolutePath(), false);
	table_->ScrollToAndSelect(select_path);
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

void App::RegisterShortcuts() {
	auto *shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Up), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	connect(shortcut, &QShortcut::activated, this, &App::GoUp);
	
	shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_H), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	
	connect(shortcut, &QShortcut::activated, [=] {
		prefs_.show_hidden_files = !prefs_.show_hidden_files;
		GoTo(current_dir_, true);
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
		MutexGuard guard(&table_model_->files()->mutex);
		table_->SelectAllFilesNTS(true, indices);
		table_model_->UpdateIndices(indices);
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

void App::TellUser(const QString &msg, const QString title) {
	QMessageBox::warning(this, title, msg);
}

}
