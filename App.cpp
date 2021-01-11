// This has to be the first include otherwise gdbusintrospection.h causes an error.
extern "C" {
#include <dconf.h>
};

#include "App.hpp"
#include "AutoDelete.hh"
#include "io/io.hh"
#include "io/File.hpp"
#include "gui/Location.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "gui/ToolBar.hpp"

#include <chrono>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>

namespace cornus {

App::App(): app_icon_(QLatin1String(":/resources/cornus.png"))
 {
	SetupIconNames();
	SetupEnvSearchPaths();
	CreateGui();
	GoToInitialDir();
	RegisterShortcuts();
	setWindowIcon(app_icon_);
	resize(800, 600);
}

App::~App() {
	QMapIterator<QString, QIcon*> i(icon_set_);
	while (i.hasNext()) {
		i.next();
		QIcon *icon = i.value();
		delete icon;
	}
}

void App::CreateGui() {
	table_model_ = new gui::TableModel(this);
	table_ = new gui::Table(table_model_);
	setCentralWidget(table_);
	
	toolbar_ = new gui::ToolBar(this);
	location_ = toolbar_->location();
	addToolBar(toolbar_);
}

void App::DisplayMime(io::File *file)
{
	QString full_path = file->build_full_path();
	QMimeType mt = mime_db_.mimeTypeForFile(full_path);
	QString mime = mt.name();
	QMessageBox::information(this, "Mime Type", mime);
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
			max_len = rect.width();
		}
	}
	
	// 300 <= width <= 900
	max_len = std::min(900, std::max(300, max_len));
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
			DisplayMime(file);
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
	files.show_hidden_files = prefs_.show_hidden_files;
	files.dir_path = dir_path;
	
	if (io::ListFiles(files) != io::Err::Ok) {
		return false;
	}
	
	int count = files.vec.size();
	table_model_->SwitchTo(files);
	current_dir_ = dir_path;
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
	QStringList args = QCoreApplication::arguments();
	QString path;
	
	if (args.size() <= 1) {
		path = QDir::homePath();
	} else {
		QString name = args[1];
		if (!name.startsWith('/')) {
			path = QCoreApplication::applicationDirPath() + '/' + name;
		} else {
			path = name;
		}
	}
	
	GoTo(path);
}

void App::GoHome() {
	GoTo(QDir::homePath());
}

void App::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	
	if (!parent.exists())
		return;
	
	QString parent_dir = parent.absolutePath();
	bool same;

	if (io::SameFiles(parent_dir, current_dir_, same) != io::Err::Ok)
		return;
	
	if (!same) {
		if (!GoTo(parent_dir))
			return;
	}
	
	table_->ScrollToAndSelect(full_path);
}

void App::GoUp() {
	if (current_dir_.isEmpty())
		return;
	
	QDir dir(current_dir_);
	
	if (!dir.cdUp())
		return;
	
	GoTo(dir.absolutePath());
	
	QScrollBar *sb = table_->verticalScrollBar();
	sb->setValue(0);
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
			IconByTruncName(file, QLatin1String("special_folder"), &icon_cache_.folder);
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

void App::RegisterShortcuts() {
	auto *shortcut = new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Up), this);
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
		if (index == -1 || index == 0 || index == name.size() - 1)
			continue;
		
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

}
