#include "Tab.hpp"

#include "actions.hxx"
#include "../App.hpp"
#include "AttrsDialog.hpp"
#include "../AutoDelete.hh"
#include "CountFolder.hpp"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "../Hid.hpp"
#include "../Prefs.hpp"
#include "../History.hpp"
#include "IconView.hpp"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "Location.hpp"
#include "../misc/Blacklist.hpp"
#include "OpenOrderPane.hpp"
#include "../str.hxx"
#include "Table.hpp"
#include "TableModel.hpp"
#include "TabsWidget.hpp"
#include "TreeModel.hpp"
#include "TreeView.hpp"
#include "../ui.hh"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QDrag>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QUrl>

//#define CORNUS_WAITED_FOR_WIDGETS

namespace cornus {

const QVector<QString> ArchiveExtensions = {
	QLatin1String("zip"), QLatin1String("xz"), QLatin1String("gz"),
	QLatin1String("odt"), QLatin1String("ods"), QLatin1String("rar"),
	QLatin1String("7z"), QLatin1String("tar"), QLatin1String("iso"),
	QLatin1String("bz2"), QLatin1String("lz"), QLatin1String("lz4"),
	QLatin1String("lzma"), QLatin1String("lzo"), QLatin1String("z"),
	QLatin1String("jar"), QLatin1String("zst"), QLatin1String("dmg")
};

void OpenWith::Clear()
{
	for (auto *next: show_vec) {
		delete next;
	}
	
	for (auto *next: hide_vec) {
		delete next;
	}
	
	show_vec.clear();
	hide_vec.clear();
}

void FigureOutSelectPath(QString &select_path, QString &go_to_path)
{
	if (select_path.isEmpty())
		return;
	
	if (go_to_path.isEmpty())
	{
/// @go_to_path is empty
/// @select_path is absolute path
		if (!io::FileExists(select_path))
		{
			QString msg = "File to select doesn't exist:\n\""
				+ select_path + QChar('\"');
			mtl_printq(msg);
			select_path.clear();
			return;
		}
		
		QDir parent_dir(select_path);
		if (!parent_dir.cdUp())
		{
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
	if (!params->reload)
	{
		if (tab->ViewIsAt(params->dir_path.path))
		{
			delete params;
			return nullptr;
		}
	}

	if (!params->dir_path.path.endsWith('/'))
		params->dir_path.path.append('/');
	
	io::FilesData *new_data = new io::FilesData();
	new_data->reloaded(params->reload);
	io::Files &files = tab->view_files();
	{
		auto g = files.guard();
		new_data->sorting_order = files.data.sorting_order;
	}

	new_data->action = params->action;
	new_data->start_time = std::chrono::steady_clock::now();
	new_data->show_hidden_files(params->show_hidden_files);
	new_data->unprocessed_dir_path = params->dir_path.path;
	if (params->dir_path.processed == Processed::Yes)
		new_data->processed_dir_path = params->dir_path.path;

	const auto cdf = params->count_dir_files ?
		io::CountDirFiles::Yes : io::CountDirFiles::No;
	
	if (!io::ListFiles(*new_data, &files, app->env(), cdf, &app->possible_categories()))
	{
		delete params;
		delete new_data;
		return nullptr;
	}
	
	#ifdef CORNUS_WAITED_FOR_WIDGETS
		using Clock = std::chrono::steady_clock;
		auto start_time = Clock::now();
	#endif
	GuiBits &gui_bits = app->gui_bits();
	{
		auto guard = gui_bits.guard();
		while (!gui_bits.created())
		{
			gui_bits.CondWait();
		}
	}
	#ifdef CORNUS_WAITED_FOR_WIDGETS
		if (files.first_time)
		{
			auto now = std::chrono::steady_clock::now();
			const float elapsed = std::chrono::duration<float,
				std::chrono::milliseconds::period>(now - start_time).count();
			mtl_info("Waited for gui creation: %.1f ms", elapsed);
		}
	#endif
	
	QMetaObject::invokeMethod(tab, "GoToFinish",
		Q_ARG(cornus::io::FilesData*, new_data));
	delete params;
	
	return nullptr;
}

bool SendURLsClipboard(QList<QUrl> list, io::Message msg_id) {
	auto ba = new ByteArray();
	ba->set_msg_id(msg_id);
	for (const auto &next: list)
	{
		ba->add_string(next.toLocalFile());
	}
	return io::socket::SendAsync(ba);
}

namespace gui {

Tab::Tab(App *app, const TabId tab_id) : app_(app), id_(tab_id)
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
	open_with_.Clear();
	delete undo_delete_menu_;
	undo_delete_menu_ = nullptr;
}

void Tab::ActionCopy()
{
	QMimeData *mime = CreateMimeWithSelectedFiles(ClipboardAction::Copy);
	QGuiApplication::clipboard()->setMimeData(mime);
}

void Tab::ActionCopyPaths(const Path path)
{
	auto list = FetchFilePaths(path, WhichFiles::Selected);
	QString s;
	cauto count = list.size();
	
	for (int i = 0; i < count; i++) {
		cauto next = list.at(i);
		s.append(next);
		if (i < (count - 1)) {
			s.append('\n');
		}
	}
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(s);
}

void Tab::ActionCut()
{
	QMimeData *mime = CreateMimeWithSelectedFiles(ClipboardAction::Cut);
	QGuiApplication::clipboard()->setMimeData(mime);
}

void Tab::ActionPaste()
{
	const ClipboardData cb_data = App::GetClipboardData();
	if (cb_data.action == ClipboardAction::None) {
		return;
	}
	
	io::Message io_op = io::Message::Pasted_Hint;
	
	if (cb_data.action == ClipboardAction::Cut) {
		io_op |= io::Message::Move;
	} else {
		io_op |= io::Message::Copy;
		io_op |= io::Message::DontTryAtomicMove;
	}
	
	QString to_dir = app_->tab()->current_dir();
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		MTL_CHECK_VOID(hash_info.valid());
	}
	
	auto *ba = new ByteArray();
	if (needs_root)
	{
		ba->add_u64(hash_info.num);
	}
	ba->set_msg_id(io_op);
	ba->add_string(to_dir);
	QVector<QString> names;
	for (cauto url: cb_data.urls)
	{
		QString as_str = url.toString();
		ba->add_string(as_str);
		QString full_path = url.toLocalFile();
		QString filename = io::GetFileNameOfFullPath(full_path).toString();
		if (!filename.isEmpty())
			names.append(filename);
	}
	
	/// Not using qclipboard->clear() because it doesn't work:
	QGuiApplication::clipboard()->setMimeData(new QMimeData());
	
	view_files().SelectFilenamesLater(names, SameDir::Yes);
	io::socket::SendAsync(ba, socket_path);
}

void Tab::ActionPasteLinks(const LinkType link)
{
	const ClipboardData cb_data = App::GetClipboardData();
	if (cb_data.action == ClipboardAction::None) {
		return;
	}
	
	QString err;
	QVector<QString> names;
	QString to_dir_path = app_->tab()->current_dir();
	
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir_path, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		MTL_CHECK_VOID(hash_info.valid());
		
		auto *ba = new ByteArray();
		ba->add_u64(hash_info.num);
		const auto msg = (link == LinkType::Absolute) ? io::Message::PasteLinks
			: io::Message::PasteRelativeLinks;
		ba->set_msg_id(msg);
		ba->add_string(to_dir_path);
		
		for (cauto &url: cb_data.urls)
		{
			ba->add_string(url.toString());
			QString full_path = url.toLocalFile();
			QString filename = io::GetFileNameOfFullPath(full_path).toString();
			
			if (!filename.isEmpty())
				names.append(filename);
		}
		
		io::socket::SendAsync(ba, socket_path);
	} else {
		if (link == LinkType::Absolute)
			io::PasteLinks(cb_data.urls, to_dir_path, &names, &err);
		else if (link == LinkType::Relative)
			io::PasteRelativeLinks(cb_data.urls, to_dir_path, &names, &err);
		else {
			mtl_trace();
			return;
		}
		
		/// Not using qclipboard->clear() because it doesn't work:
		QGuiApplication::clipboard()->setMimeData(new QMimeData());
		
		if (!err.isEmpty()) {
			app_->TellUser(tr("Paste Link Error: ") + err);
		}
	}
	
	view_files().SelectFilenamesLater(names, SameDir::Yes);
}

/** void Daemon::CopyURLsToClipboard(ByteArray *ba)
{
	QMimeData *mime = new QMimeData();
	
	QList<QUrl> urls;
	QString prefix = QLatin1String("file://");
	while (ba->has_more())
	{
		QString s = prefix + ba->next_string();
		mtl_printq2("URL: ", s);
		urls.append(QUrl(s));
	}
	
	mime->setUrls(urls);
	QClipboard *clip = QApplication::clipboard();
	clip->setMimeData(mime);
	mtl_info("Done");
}

void Daemon::CutURLsToClipboard(ByteArray *ba)
{
	QMimeData *mime = new QMimeData();
	
	QList<QUrl> urls;
	while (ba->has_more()) {
		urls.append(QUrl(ba->next_string()));
	}
	
	mime->setUrls(urls);
	
	QByteArray kde_mark;
	char c = '1';
	kde_mark.append(c);
	mime->setData(str::KdeCutMime, kde_mark);
	
	QApplication::clipboard()->setMimeData(mime);
} **/

void Tab::ActionPlayInMpv()
{
	auto paths = io::MergeList(FetchFilePaths(Path::Full, WhichFiles::Selected), '\n');
	if (paths.isEmpty())
		return;
	
	QString dir = io::GetLastingTmpDir();
	if (dir.isEmpty())
		return;
	
	QString filepath = dir + QLatin1String("/cornus.m3u");
	auto buf = paths.toLocal8Bit();
	mtl_check_void(io::WriteToFile(filepath, buf.data(), buf.size()));
	QProcess::startDetached(QLatin1String("mpv"), QStringList(filepath));
}

void Tab::AddIconsView()
{
	if (icon_view_)
		return;
	
	QWidget *w = new QWidget();
	QBoxLayout *row = new QBoxLayout(QBoxLayout::LeftToRight);
	w->setLayout(row);
	QScrollBar *vs = new QScrollBar(Qt::Vertical);
	icon_view_ = new IconView(app_, this, vs);
	row->addWidget(icon_view_);
	row->addWidget(vs);
	icons_view_index_ = viewmode_stack_->addWidget(w);
}

void Tab::AddOpenWithMenuTo(QMenu *main_menu, const QString &full_path)
{
	QVector<QAction*> open_with_list = CreateOpenWithList(full_path);
	auto *open_with_menu = new QMenu(tr("Open &With"));
	
	for (auto &next: open_with_list) {
		open_with_menu->addAction(next);
	}
	
	open_with_menu->addSeparator();
	QAction *action = new QAction(tr("Customize..."));
	connect(action, &QAction::triggered, [=] {
		OpenOrderPane pane(app_, this);
	});
	open_with_menu->addAction(action);
	main_menu->addMenu(open_with_menu);
}

void Tab::AllowEfaInSelectedFiles(Efa allowed_list)
{
	auto  &files = view_files();
	MutexGuard guard = files.guard();
	struct statx stx;
	PrintErrors pe = PrintErrors::No;
	for (io::File *next: files.data.vec)
	{
		if (!next->is_selected())
			continue;
		
		Efa what_changed = app_->blacklist().Allow(next, allowed_list);
		if (EfaContains(what_changed, Efa::Thumbnail)) {
			cbool ok = io::ReloadMeta(*next, stx, app_->env(), pe);
			// mtl_info("ReloadMeta result: %d for %s", ok, qPrintable(next->name()));
		}
	}
	
	app_->blacklist().Save();
}

bool Tab::AnyArchive(const QVector<QString> &extensions) const
{
	for (auto &next: ArchiveExtensions)
	{
		if (extensions.contains(next))
			return true;
	}
	
	return false;
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
	
	layout->addWidget(viewmode_stack_);
}

QMimeData* Tab::CreateMimeWithSelectedFiles(const ClipboardAction action)
{
	auto &files = view_files();
	MutexGuard guard = files.guard();
	auto *mime = new QMimeData();
	
	io::FileBits flag = io::FileBits::Empty;
	if (action == ClipboardAction::Cut) {
		flag = io::FileBits::ActionCut;
		mime->setData(str::KdeCutMime, "1");
	} else if (action == ClipboardAction::Copy)
		flag = io::FileBits::ActionCopy;
	else if (action == ClipboardAction::Paste)
		flag = io::FileBits::ActionPaste;
	else if (action == ClipboardAction::PasteLink)
		flag = io::FileBits::PasteLink;
	
	QList<QUrl> urls;
	QSet<int> indices;
	int i = -1;
	for (io::File *next: files.data.vec)
	{
		i++;
		
		if (next->is_selected()) {
			indices.insert(i);
			next->toggle_flag(flag, true);
			QString s = next->build_full_path();
			urls.append(QUrl::fromLocalFile(s));
		} else {
			if (next->clear_all_actions_if_needed())
				indices.insert(i);
		}
	}
	
	mime->setUrls(urls);
	
	return mime;
}

QVector<QAction*>
Tab::CreateOpenWithList(const QString &full_path)
{
	open_with_.full_path = full_path;
	open_with_.mime = app_->QueryMimeType(full_path);
	ReloadOpenWith();
	QVector<QAction*> ret;
	const QLocale locale = QLocale::system();
	for (DesktopFile *next: open_with_.show_vec)
	{
		QString name = next->GetName(locale);
		QString generic = next->GetGenericName(locale);
		if (!generic.isEmpty()) {
			name += QLatin1String(" (") + generic + ')';
		}
		
//		QString priority_str = QString::number((i8)next->priority());
//		name += " (↑" + priority_str + ")";
		QAction *action = new QAction(name);
		QVariant v = QVariant::fromValue((void *) next);
		action->setData(v);
		action->setIcon(next->CreateQIcon());
		connect(action, &QAction::triggered, this, &Tab::LaunchFromOpenWithMenu);
		ret.append(action);
	}
	
	return ret;
}

QString Tab::CurrentDirTrashPath()
{
	QString full_path = current_dir_;
	if (!full_path.endsWith('/'))
		full_path.append('/');
	full_path.append(trash::name());
	
	return full_path;
}

void Tab::DeleteSelectedFiles(const ShiftPressed sp)
{
	QVector<QString> paths;
	{
		io::Files &files = this->view_files();
		MutexGuard guard = files.guard();
		
		for (io::File *next: files.data.vec)
		{
			if (next->is_selected())
				paths.append(next->build_full_path());
		}
	}
	
	if (paths.isEmpty())
		return;
	
	const auto msg_id = (sp == ShiftPressed::Yes) ? io::Message::DeleteFiles
		: io::Message::MoveToTrash;
	
	QString dir_path = io::GetParentDirPath(paths[0]).toString();
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(dir_path, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
mtl_trace("Starting up root daemon");
		hash_info = app_->WaitForRootDaemon(CanOverwrite::No);
		MTL_CHECK_VOID(hash_info.valid());
	}
	
	ByteArray *ba = new ByteArray();
	if (needs_root)
		ba->add_u64(hash_info.num);
	
	ba->set_msg_id(msg_id);
	for (auto &next: paths)
	{
		ba->add_string(next);
	}
	
	io::socket::SendAsync(ba, socket_path);
}

void Tab::DisplayingNewDirectory(const DirId dir_id, const Reload r)
{
	if (icon_view_ != nullptr)
		icon_view_->DisplayingNewDirectory(dir_id, r);
}

void Tab::DragEnterEvent(QDragEnterEvent *evt)
{
	const ui::DndType dt = ui::GetDndType(evt->mimeData());
	if (dt != ui::DndType::None)
		evt->acceptProposedAction();
}

void Tab::DropEvent(QDropEvent *evt, const ForceDropToCurrentDir fdir)
{
	const QMimeData *md = evt->mimeData();
	const ui::DndType dnd_type = ui::GetDndType(md);
	io::File *to_dir = nullptr;
	
	if (fdir == ForceDropToCurrentDir::Yes)
	{
		to_dir = io::FileFromPath(current_dir());
	} else {
		auto &files = view_files();
		MutexGuard guard = files.guard();
		
		if (view_mode_ == ViewMode::Details)
		{
			if (table_->GetFileAt_NoLock(evt->position().toPoint(), PickedBy::VisibleName, &to_dir) != -1
				&& to_dir->is_dir_or_so())
			{
				to_dir = to_dir->Clone();
			} else {
				to_dir = io::FileFromPath(current_dir());
			}
		} else if (view_mode_ == ViewMode::Icons) {
			to_dir = icon_view_->GetFileAt_NoLock(evt->position().toPoint(), Clone::Yes);
		} else {
			/// Otherwise drop onto current directory:
			to_dir = io::FileFromPath(current_dir());
		}
	}
	
	MTL_CHECK_VOID(to_dir != nullptr);
	AutoDelete ad(to_dir);
	
	if (dnd_type == ui::DndType::Ark)
	{
		const QString dbus_service_key = QLatin1String("application/x-kde-ark-dndextract-service");
		const QString dbus_path_key = QLatin1String("application/x-kde-ark-dndextract-path");
		const QString dbus_client = md->data(dbus_service_key);
		const QString dbus_path = md->data(dbus_path_key);
		const QString to_dir_path = to_dir->build_full_path();
		QUrl url = QUrl::fromLocalFile(to_dir_path);
		
		QDBusMessage msg = QDBusMessage::createMethodCall(dbus_client, dbus_path,
			QLatin1String("org.kde.ark.DndExtract"), QLatin1String("extractSelectedFilesTo"));
		msg.setArguments({url.toDisplayString(QUrl::PreferLocalFile)});
		QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
	} else if (dnd_type == ui::DndType::Urls) {
		QVector<io::File*> *files_vec = new QVector<io::File*>();
		
		for (const QUrl &url: evt->mimeData()->urls())
		{
			io::File *file = io::FileFromPath(url.path());
			if (file != nullptr)
				files_vec->append(file);
		}
		ExecuteDrop(files_vec, to_dir, evt->proposedAction(), evt->possibleActions());
	} else {
		mtl_warn();
	}
}

void Tab::ExecuteDrop(QVector<io::File*> *files_vec,
	io::File *to_dir, Qt::DropAction drop_action,
	Qt::DropActions possible_actions)
{
	MTL_CHECK_VOID(files_vec != nullptr);
	const QString to_dir_path = to_dir->build_full_path();
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir_path, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		MTL_CHECK_VOID(hash_info.valid());
	}
	
	io::Message io_operation = io::MessageFor(drop_action)
		| io::MessageForMany(possible_actions);
	auto *ba = new ByteArray();
	
	if (needs_root) {
		ba->add_u64(hash_info.num);
	}
	
	ba->set_msg_id(io_operation);
	ba->add_string(to_dir_path);
	//mtl_info("to_dir_path: %s", qPrintable(to_dir_path));
	
	for (io::File *next: *files_vec) {
		//mtl_info("build_full_path(): %s", qPrintable(next->build_full_path()));
		ba->add_string(next->build_full_path());
	}
	
	io::socket::SendAsync(ba, socket_path);
}

QStringList Tab::FetchFilePaths(const Path path, const WhichFiles wf)
{
	QStringList list;
	auto &files = view_files();
	{
		auto g = files.guard();
		for (io::File *next: files.data.vec)
		{
			if (wf == WhichFiles::All || next->is_selected()) {
				const auto s = (path == Path::Full) ? next->build_full_path() : next->name();
				list.append(s);
			}
		}
	}
	
	return list;
}

QList<PathAndMode> Tab::FetchFilesInfo(const WhichFiles wf)
{
	QList<PathAndMode> list;
	auto &files = view_files();
	{
		auto g = files.guard();
		for (io::File *next: files.data.vec)
		{
			if (wf == WhichFiles::All || next->is_selected()) {
				list.append(next->path_and_mode());
			}
		}
	}
	
	return list;
}

int Tab::GetScrollValue() const
{
	if (view_mode_ == ViewMode::Details)
	{
		return table_->scrollbar()->value();
	} else if (view_mode_ == ViewMode::Icons) {
		return icon_view_->scrollbar()->value();
	} else {
		mtl_trace();
		return -1;
	}
}

int Tab::GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file,
	QString *full_path)
{
	io::Files &files = view_files();
	MutexGuard guard = files.guard();

	io::File *file = nullptr;
	int file_index;
	if (view_mode_ == ViewMode::Details) {
		file_index = table_->GetFileAt_NoLock(local_pos, PickedBy::VisibleName, &file);
	} else {
		file = icon_view_->GetFileAt_NoLock(local_pos, Clone::Yes, &file_index);
	}
	
	if (file_index != -1 && ret_cloned_file != nullptr)
		*ret_cloned_file = file->Clone();
	
	if (full_path != nullptr)
		*full_path = file->build_full_path();

	return file_index;
}

int Tab::GetVisibleFileIndex()
{
	// Used i.e. when switching to a different view to approximately
	// scroll to the file the user was at in the previous view.
	
	switch (view_mode_) {
	case ViewMode::Details: return table_->GetVisibleFileIndex();
	case ViewMode::Icons: return icon_view_->GetVisibleFileIndex();
	default: return -1;
	}
}

void Tab::GetSelectedArchives(QVector<QString> &urls)
{
	io::Files &files = view_files();
	MutexGuard guard = files.guard();
	
	for (io::File *next: files.data.vec) {
		if (!next->is_selected() || !next->is_regular())
			continue;
		
		QString ext = next->cache().ext;
		if (ext.isEmpty())
			continue;
		
		if (ArchiveExtensions.contains(ext)) {
			urls.append(QUrl::fromLocalFile(next->build_full_path()).toString());
		}
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
	params->reload = (r == Reload::Yes);
	params->action = action;
	auto &prefs = app_->prefs();
	params->show_hidden_files = prefs.show_hidden_files();
	params->count_dir_files = prefs.show_dir_file_count();
	io::NewThread(cornus::GoToTh, params);
	
	return true;
}

void Tab::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	MTL_CHECK_VOID(parent.exists());
	QString parent_dir = parent.absolutePath();
	const QString name = io::GetFileNameOfFullPath(full_path).toString();
	const SameDir same_dir = io::SameFiles(parent_dir, current_dir_) ? SameDir::Yes : SameDir::No;
	auto &files = view_files();
	files.SelectFilenamesLater({name}, same_dir);
	
	if (same_dir == SameDir::No) {
		MTL_CHECK_VOID(GoTo(Action::To, {parent_dir, Processed::No}, Reload::No));
	} else {
		files.WakeUpInotify(Lock::Yes);
	}
}

void Tab::GoToFinish(cornus::io::FilesData *new_data)
{
	if (new_data->action != Action::Back)
	{
		history_->Record();
	}
	
	AutoDelete ad(new_data);
	//cint new_file_count = new_data->vec.size();
	table_->ClearMouseOver();
/// current_dir_ must be assigned before model->SwitchTo
/// otherwise won't properly show current partition's free space
	current_dir_ = new_data->processed_dir_path;
	bool got_files_to_select;
	{
		io::Files &files = view_files();
		got_files_to_select = !files.data.filenames_to_select.isEmpty();
	}
	
	table_model_->SwitchTo(new_data);
	history_->Add(new_data->action, current_dir_);
	
	if (new_data->action == Action::Back)
	{
		QVector<QString> list;
		history_->GetSelectedFiles(list);
		table_->SelectByLowerCase(list, NamesAreLowerCase::Yes);
	} else if (new_data->scroll_to_and_select.isEmpty()) {
		if (!new_data->reloaded() && !got_files_to_select)
			ScrollToFile(0);
	}
	
	QString dir_name = QDir(new_data->processed_dir_path).dirName();
	if (dir_name.isEmpty())
		dir_name = new_data->processed_dir_path; // likely "/"
	
	using Clock = std::chrono::steady_clock;
	if (app_->prefs().show_ms_files_loaded() &&
		(new_data->start_time != std::chrono::time_point<Clock>::max()))
	{
		auto now = std::chrono::steady_clock::now();
		list_speed_ = std::chrono::duration<float,
			std::chrono::milliseconds::period>(now - new_data->start_time).count();
	} else {
		list_speed_ = -1;
	}
	
	SetTitle(dir_name);
	app_->SelectCurrentTab();
	
	// Slots are called when a signal connected to it is emitted. They are
	// regular C++ functions and can be called normally, their only special
	// feature is that signals can be connected to them.
	Q_EMIT SwitchedToNewDir(new_data->unprocessed_dir_path, new_data->processed_dir_path);
}

void Tab::GoToInitialDir()
{
	const QStringList args = QCoreApplication::arguments();
	if (args.size() <= 1)
	{
		GoHome();
		return;
	}
	
	struct Commands {
		QString select;
		QString go_to_path;
	} cmds = {};
	
	const QString SelectCmdName = QLatin1String("select");
	const QString CmdPrefix = QLatin1String("--");
	cint arg_count = args.size();
	
	ViewMode view = ViewMode::None;
	const QString IconsViewStr = QLatin1String("--view=icons");
	const QString DetailsViewStr = QLatin1String("--view=details");
	QString *next_command = nullptr;
	for (int i = 1; i < arg_count; i++)
	{
		const QString &next = args[i];
		
		if (view == ViewMode::None)
		{
			const QString s = next.toLower();
			if (s == IconsViewStr)
				view = ViewMode::Icons;
			else if (s == DetailsViewStr)
				view = ViewMode::Details;
		}
		
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
	
	if (view != ViewMode::None)
	{
		SetViewMode(view);
	}
	
	if (cmds.go_to_path.isEmpty())
		cmds.go_to_path = QDir::homePath();
	
	FigureOutSelectPath(cmds.select, cmds.go_to_path);
	io::FileType file_type;
	if (!io::FileExists(cmds.go_to_path, &file_type))
	{
		QString name = cmds.select;
		if (name.startsWith('/'))
			name = io::GetFileNameOfFullPath(name).toString();
		view_files().SelectFilenamesLater({name});
		GoTo(Action::To, {QDir::homePath(), Processed::No}, Reload::No);
		return;
	}
	
	if (file_type == io::FileType::Dir)
	{
		QString name = cmds.select;
		if (!name.isEmpty())
		{
			if (name.startsWith('/'))
				name = io::GetFileNameOfFullPath(name).toString();
			view_files().SelectFilenamesLater({name}, SameDir::Yes);
		}
		
		GoTo(Action::To, {cmds.go_to_path, Processed::No}, Reload::No);
	} else {
		QDir parent_dir(cmds.go_to_path);
		if (!parent_dir.cdUp())
		{
			QString msg = "Can't access parent dir of file:\n\""
				+ cmds.go_to_path + QChar('\"');
			mtl_printq(msg);
			GoHome();
			return;
		}
		
		QString name = io::GetFileNameOfFullPath(cmds.go_to_path).toString();
		view_files().SelectFilenamesLater({name}, SameDir::Yes);
		GoTo(Action::To, {parent_dir.absolutePath(), Processed::No}, Reload::No);
		return;
	}
}

void Tab::GoToSimple(QStringView full_path) {
	GoTo(Action::To, {full_path.toString(), Processed::No});
}

void Tab::GoUp()
{
	if (current_dir_.isEmpty())
		return;
	
	QDir dir(current_dir_);
	
	if (!dir.cdUp())
		return;
	
	QString name = io::GetFileNameOfFullPath(current_dir_).toString();
	view_files().SelectFilenamesLater({name});
	GoTo(Action::Up, {dir.absolutePath(), Processed::Yes}, Reload::No);
}

void Tab::FocusView()
{
	switch (view_mode_) {
	case ViewMode::Details: {
		table_->setFocus(Qt::MouseFocusReason);
		break;
	}
	case ViewMode::Icons: {
		icon_view_->setFocus(Qt::MouseFocusReason);
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

void Tab::HandleMouseRightClickSelection(const QPoint &pos, QSet<int> &indices)
{
	io::Files &files = view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	gui::ShiftSelect *shift_select = nullptr;
	int file_index =  -1;
	if (view_mode_ == ViewMode::Details)
	{
		file_index = table_->GetFileAt_NoLock(pos, PickedBy::VisibleName, &file);
		shift_select = table_->shift_select();
	} else if (view_mode_ == ViewMode::Icons) {
		file = icon_view_->GetFileAt_NoLock(pos, Clone::No, &file_index);
		shift_select = icon_view_->shift_select();
	} else {
		mtl_tbd();
	}
	
	if (file == nullptr) {
		files.SelectAllFiles(Lock::No, Selected::No, indices);
	} else {
		if (!file->is_selected()) {
			files.SelectAllFiles(Lock::No, Selected::No, indices);
			file->set_selected(true);
			if (shift_select)
				shift_select->base_row = file_index;
			indices.insert(file_index);
		}
	}
}

gui::IconView* Tab::icon_view() {
	if (!icon_view_) {
		AddIconsView();
	}
	return icon_view_;
}

void Tab::Init()
{
	setFocusPolicy(Qt::WheelFocus);
	// enables receiving ordinary mouse events (when mouse is not down)
	setMouseTracking(true);
	
	notify_.Init();
	files_id_ = app_->GenNextFilesId();
	history_ = new History(app_);
	CreateGui();
}

void Tab::KeyPressEvent(QKeyEvent *evt)
{
	auto  &files = view_files();
	cint key = evt->key();
	auto *app = app_;
	cauto modifiers = evt->modifiers();
	cbool any_modifiers = (modifiers != Qt::NoModifier);
	cbool shift = (modifiers & Qt::ShiftModifier);
	cbool ctrl = (modifiers & Qt::ControlModifier);
	QSet<int> indices;
	
	if (!any_modifiers)
	{
		if (key >= Qt::Key_1 && key <= Qt::Key_9) {
			cint index = key - Qt::Key_0 - 1;
			app_->SelectTabAt(index, FocusView::Yes);
			return;
		}
		
		if (key == Qt::Key_G) {
			ToggleMagnifiedMode();
		} else if (key == Qt::Key_F) {
			TellEfaOfSelectedFile();
		}
	}
	
	if (ctrl) {
		if (key == Qt::Key_C)
			ActionCopy();
		else if (key == Qt::Key_X)
			ActionCut();
		else if (key == Qt::Key_V) {
			ActionPaste();
		} else if (key == Qt::Key_R) {
			app_->Reload();
		}
	}
	
	if (key == Qt::Key_Delete) {
		DeleteSelectedFiles(shift ? ShiftPressed::Yes : ShiftPressed::No);
	} else if (key == Qt::Key_F1) {
		MarkLastWatchedFile();
		UpdateView();
	} else if (key == Qt::Key_F2) {
		app_->RenameSelectedFile();
	} else if (key == Qt::Key_F3) {
		MarkSelectedFilesAsWatched();
	} else if (key == Qt::Key_Return) {
		if (!any_modifiers) {
			io::File *cloned_file = nullptr;
			cint row = files.GetFirstSelectedFile(Lock::Yes, &cloned_file, Clone::Yes);
			if (row != -1) {
				app->FileDoubleClicked(cloned_file, PickedBy::VisibleName);
				indices.insert(row);
			}
		}
	} else if (key == Qt::Key_Down || key == Qt::Key_Right) {
		if (shift)
			app_->hid()->HandleKeyShiftSelect(this, VDirection::Down, key);
		else
			app_->hid()->HandleKeySelect(this, VDirection::Down, key, modifiers);
	} else if (key == Qt::Key_Up || key == Qt::Key_Left) {
		if (shift)
			app_->hid()->HandleKeyShiftSelect(this, VDirection::Up, key);
		else
			app_->hid()->HandleKeySelect(this, VDirection::Up, key, modifiers);
	} else if (key == Qt::Key_D) {
		if (any_modifiers)
			return;
		io::File *cloned_file = nullptr;
		int row = files.GetFirstSelectedFile(Lock::Yes, &cloned_file, Clone::Yes);
		if (row != -1)
			app_->DisplayFileContents(row, cloned_file);
		else {
			app_->SetTopLevel(TopLevel::Browser);
		}
	} else if (key == Qt::Key_V) {
		if (!any_modifiers)
			SetNextView();
	} else if (key == Qt::Key_Escape) {
		app_->SetTopLevel(TopLevel::Browser);
	} else if (key == Qt::Key_PageUp) {
		auto *vs = ViewScrollBar();
		MTL_CHECK_VOID(vs != nullptr);
		cint rh = ViewRowHeight();
		cint page = vs->pageStep() - rh * 2;
		cint new_val = vs->value() - page;
		vs->setValue(new_val);
		int r = new_val / rh;
		if (new_val % rh != 0)
			r++;
		int row = std::max(0, r);
		app_->hid()->SelectFileByIndex(this, row, DeselectOthers::Yes);
	} else if (key == Qt::Key_PageDown) {
		auto *vs = ViewScrollBar();
		MTL_CHECK_VOID(vs != nullptr);
		cint rh = ViewRowHeight();
		cint page = vs->pageStep() - rh;
		cint new_val = vs->value() + page;
		vs->setValue(new_val);
		int new_val2 = new_val + page;
		cint file_count = files.cached_files_count;
		int row = std::min(file_count - 1, new_val2 / rh);
		app_->hid()->SelectFileByIndex(this, row, DeselectOthers::Yes);
	} else if (key == Qt::Key_Home) {
		QScrollBar *vs = ViewScrollBar();
		MTL_CHECK_VOID(vs != nullptr);
		vs->setValue(0);
		app_->hid()->SelectFileByIndex(this, 0, DeselectOthers::Yes);
	} else if (key == Qt::Key_End) {
		auto *vs = ViewScrollBar();
		MTL_CHECK_VOID(vs != nullptr);
		vs->setValue(vs->maximum());
		const auto count = view_files().cached_files_count;
		app_->hid()->SelectFileByIndex(this, count - 1, DeselectOthers::Yes);
	} else if (key == Qt::Key_M && !any_modifiers) {
		io::File *cloned_file = nullptr;
		files.GetFirstSelectedFile(Lock::Yes, &cloned_file, Clone::Yes);
		if (cloned_file)
			AttrsDialog attrs_d(app_, cloned_file);
	}
	
	UpdateIndices(indices);
}

void Tab::LaunchFromOpenWithMenu()
{
	QAction *act = qobject_cast<QAction *>(sender());
	QVariant v = act->data();
	DesktopFile *p = (DesktopFile*) v.value<void *>();
	DesktopArgs args;
	args.full_path = open_with_.full_path;
	args.working_dir = current_dir();
	p->Launch(args);
}

QString Tab::ListSpeedString() const
{
	QString diff_str = io::FloatToString(list_speed_, 2);
	QString num_files = QString::number(view_files().cached_files_count);
	QString ret = QLatin1String(" [") + num_files + tr(" files=")
		+ diff_str + QLatin1String("ms]");
	return ret;
}

void Tab::MarkFilesAsWatched(const enum Lock l, QList<io::File*> &vec)
{
	if (vec.isEmpty())
		return;
	
	auto &files = view_files();
	auto g = files.guard(l);
	cauto &key = media::WatchProps::Name;
	for (io::File *needle: vec) {
		cbool allowed_to_set = app_->blacklist().IsAllowed(needle, Efa::Text);
		if (!allowed_to_set) {
			continue;
		}
		for (io::File *next: files.data.vec)
		{
			if (next->id() == needle->id())
			{
				next->WatchProp(Op::Invert, media::WatchProps::Watched);
				break;
			}
		}
	}
}

void Tab::MarkLastWatchedFile()
{
	{
		auto &files = view_files();
		auto guard = files.guard(Lock::Yes);
		io::File *file;
		cint index = files.GetFirstSelectedFile(Lock::No, &file, Clone::No);
		if (index != -1)
			SetLastWatched(Lock::No, file);
	}
	UpdateView();
}

void Tab::MarkSelectedFilesAsWatched() {
	{
		auto &files = view_files();
		auto g = files.guard(Lock::Yes);
		QList<io::File*> vec = files.GetSelectedFiles(Lock::No, Clone::No);
		MarkFilesAsWatched(Lock::No, vec);
	}
	UpdateView();
}

void Tab::NotivyViewsOfFileChange(const io::FileEventType evt, io::File *cloned_file)
{
	// The detailed view processes first in TableModel::InotifyEvent()
	AutoDelete ad(cloned_file);
	// File changes are monitored only in TableModel.cpp which represents
	// the detailed view, which is why the detailed view uses
	// this method to inform the icon view of changes.
	auto *iv = icon_view();
	if (iv)
	{
		io::File *file = (cloned_file != nullptr) ? cloned_file->Clone() : nullptr;
		// it's because each view might use a new thread to load specific data
		// like thumbnail of this file and it might be in a separate thread,
		// thus each view must get its clone, and `cloned_file` must be freed at
		// this function's end no matter what.
		iv->FileChanged(evt, file);
	} else {
		mtl_warn("icon view is null!");
	}
}

void Tab::PaintMagnified(QWidget *viewport, const QStyleOptionViewItem &option)
{
	io::Files &files = view_files();
	io::File *file;
	if (files.GetFirstSelectedFile(Lock::Yes, &file, Clone::Yes) == -1)
		return;
	
	AutoDelete file_ad(file);
	QPainter p(viewport);
	QPainter *painter = &p;
	painter->setRenderHint(QPainter::Antialiasing);
	QFont fnt = option.font;
	if (fnt.pixelSize() == -1)
	{
		fnt.setPointSize(fnt.pointSize() * app_->magnify_value());
	} else {
		fnt.setPixelSize(fnt.pixelSize() * app_->magnify_value());
	}
	
	painter->setFont(fnt);
	QFontMetrics fm(fnt);
	cint h = fm.height() * 2 + 4;
	int y = (height() - h) / 2;
	QRect text_rect(2, y, width(), h);
	auto color_role = QPalette::Base;
	
	// Opacity: the value should be in the range 0.0 to 1.0, where 0.0 is fully
	// transparent and 1.0 is fully opaque.
	cint saved_opacity = painter->opacity();
	painter->setOpacity(app_->magnify_opacity());
	painter->fillRect(text_rect, option.palette.brush(color_role));
	painter->setOpacity(saved_opacity);
	
	painter->setBrush(option.palette.text());
	cint flags = Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop;
	painter->drawText(text_rect, flags, file->name());
}

void Tab::PopulateUndoDelete(QMenu *menu)
{
	menu->clear();
	QMap<i64, QVector<trash::Names>> all_items;
	MTL_CHECK_VOID(trash::ListItems(CurrentDirTrashPath(), all_items));
	if (all_items.isEmpty())
		return;
	
	const QString format = QLatin1String("yyyy/MM/dd hh:mm:ss");
	cint MaxMenuItemsToShow = 3;
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
		ci64 t = i.key();
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

void Tab::ProcessAction(const QString &action)
{
	App *app = app_;
	auto &files = this->view_files();
	
	if (action == actions::DeleteFiles) {
		int count = files.GetSelectedFilesCount(Lock::Yes);
		if (count == 0)
			return;
		
		QString question = tr("Delete PERMANENTLY ") + QString::number(count)
			+ tr(" files?");
		QMessageBox::StandardButton reply = QMessageBox::question(this,
			tr("Delete Files"), question, QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No);
		
		if (reply == QMessageBox::Yes)
			DeleteSelectedFiles(ShiftPressed::Yes);
	} else if (action == actions::RenameFile) {
		app->RenameSelectedFile();
	} else if (action == actions::OpenTerminal) {
		app->OpenTerminal();
	} else if (action == actions::RunExecutable) {
		QString ext;
		QString full_path = files.GetFirstSelectedFileFullPath(Lock::Yes, &ext);
		if (!full_path.isEmpty()) {
			ExecInfo info = app->QueryExecInfo(full_path, ext);
			if (info.is_elf() || info.is_shell_script())
				app->RunExecutable(full_path, info);
		}
	}
}

bool Tab::ReloadOpenWith()
{
	open_with_.Clear();
	int fd = io::socket::Client(cornus::SocketPath);
	if (fd == -1)
		return false;
	
	ByteArray query_ba;
	query_ba.set_msg_id(io::Message::SendOpenWithList);
	query_ba.add_string(open_with_.mime);
	
	if (!query_ba.Send(fd, CloseSocket::No))
		return false;
	
	ByteArray received_ba;
	if (!received_ba.Receive(fd))
		return false;
	
	if (!io::CheckDesktopFileABI(received_ba))
	{
		app_->TellUserDesktopFileABIDoesntMatch();
		return false;
	}
	
	while (received_ba.has_more())
	{
		const Present present = (Present)received_ba.next_i8();
		DesktopFile *next = DesktopFile::From(received_ba, app_->env());
		if (next != nullptr) {
			if (present == Present::Yes)
				open_with_.show_vec.append(next);
			else
				open_with_.hide_vec.append(next);
		}
	}
	
	return true;
}

void Tab::RemoveEfaFromSelectedFiles(Efa efa)
{
	auto  &files = view_files();
	MutexGuard guard = files.guard();
	
	for (io::File *next: files.data.vec)
	{
		if (!next->is_selected())
			continue;
		
		const Efa what_changed = app_->blacklist().Block(next, efa);
		if (EfaContains(what_changed, Efa::Thumbnail)) {
			// mtl_info("Clearing thumbnail");
			next->ClearThumbnail(io::AlsoDeleteFromDisk::Yes);
		}
	}
	
	app_->blacklist().Save();
}

void Tab::resizeEvent(QResizeEvent *ev)
{
	QWidget::resizeEvent(ev);
}

void Tab::ScrollToFile(cint file_index)
{
	if (file_index < 0)
		return;
	
	const ViewMode vm = view_mode();
	switch (vm) {
	case ViewMode::Details : {
		table_->ScrollToFile(file_index);
		break;
	}
	case ViewMode::Icons: {
		icon_view_->ScrollToFile(file_index);
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

void Tab::SetLastWatched(const enum Lock l, io::File *file)
{
	MTL_CHECK_VOID(file);
	auto &files = view_files();
	auto g = files.guard(l);
	cbool allowed_to_set =  app_->blacklist().IsAllowed(file, Efa::Text);
	for (io::File *next: files.data.vec)
	{
		if (file->id() == next->id())
		{
			if (allowed_to_set) {
				next->WatchProp(Op::Invert, media::WatchProps::LastWatched);
			}
		} else if (next->has_last_watched_attr()) {
			next->WatchProp(Op::Remove, media::WatchProps::LastWatched);
		}
	}
}

void Tab::SetNextView()
{
	ViewMode next = ViewMode::None;
	switch(view_mode_)
	{
	case ViewMode::Details: {
		next = ViewMode::Icons;
		break;
	}
	case ViewMode::Icons: {
		next = ViewMode::Details;
		break;
	}
	default: break;
	}
	
	if (next != ViewMode::None) {
		SetViewMode(next);
	}
}

void Tab::SetScrollValue(cint n)
{
	if (n == -1)
		return;
	
	if (view_mode_ == ViewMode::Details) {
		table_->scrollbar()->setValue(n);
	} else if (view_mode_ == ViewMode::Icons) {
		return icon_view_->scrollbar()->setValue(n);
	} else {
		mtl_trace();
	}
}

void Tab::SetTitle(const QString &s)
{
	title_ = s;
	QTabWidget *w = app_->tab_widget();
	int index = w->indexOf(this);
	QString short_title = title_;
	cint max_len = 10;
	if (short_title.size() > max_len)
	{
		short_title = short_title.mid(0, max_len);
		short_title += QLatin1String("..");
	}
	
	w->setTabText(index, short_title);
}

void Tab::SetViewMode(const ViewMode to_mode)
{
	cbool sync_scroll = app_->prefs().sync_views_scroll_location();
	cint last_file_index = sync_scroll ? GetVisibleFileIndex() : -1;
	view_mode_ = to_mode;
	switch (to_mode)
	{
	case ViewMode::Details: {
		viewmode_stack_->setCurrentIndex(details_view_index_);
		MTL_CHECK_VOID(table_ != nullptr);
		break;
	}
	case ViewMode::Icons: {
		if (icon_view_ == nullptr) {
			AddIconsView();
		}
		icon_view_->SetViewState(NewState::AboutToSet);
		viewmode_stack_->setCurrentIndex(icons_view_index_);
		icon_view_->SetViewState(NewState::Set);
		break;
	}
	default: {
		mtl_trace();
		return;
	}
	}
	
	FocusView();
	
	if (sync_scroll)
		ScrollToFile(last_file_index);
}

ShiftSelect* Tab::ShiftSelect()
{
	switch (view_mode_)
	{
	case ViewMode::Details: return table_->shift_select();
	case ViewMode::Icons: return icon_view_->shift_select();
	default: return nullptr;
	}
}

void Tab::ShowRightClickMenu(const QPoint &global_pos,
	const QPoint &local_pos)
{
	auto &files = view_files();
	QVector<QString> extensions;
	cint selected_count = files.GetSelectedFilesCount(Lock::Yes, &extensions);
	QMenu *menu = new QMenu();
	menu->setAttribute(Qt::WA_DeleteOnClose);
	
	const QString current_dir = this->current_dir();
	QString dir_full_path;
	QString file_under_mouse_full_path;
	io::File *file_under_mouse = nullptr;
	if (GetFileUnderMouse(local_pos, &file_under_mouse) != -1)
	{
		file_under_mouse_full_path = file_under_mouse->build_full_path();
		if (file_under_mouse->is_dir()) {
			dir_full_path = file_under_mouse_full_path;
		}
	}
	
	cornus::AutoDelete ad(file_under_mouse);
	
	if (selected_count == 1)
	{
		io::File *first_file = nullptr;
		files.GetFirstSelectedFile(Lock::Yes, &first_file, Clone::Yes);
		if (first_file != nullptr && first_file->is_dir_or_so())
		{
			QAction *action = menu->addAction(tr("Add To Bookmarks"));
			QIcon *icon = app_->GetFileIcon(first_file);
			if (icon)
				action->setIcon(*icon);
			connect(action, &QAction::triggered, [=] {
				auto &files = view_files();
				io::File *otf = nullptr;
				files.GetFirstSelectedFile(Lock::Yes, &otf, Clone::Yes);
				if (otf)
				{
					QVector<io::File*> vec = { otf };
					app_->tree_model()->AddBookmarks(vec, QPoint(1, 1));
				}
			});
		}
		
		delete first_file;
		first_file = nullptr;
		
		if (file_under_mouse != nullptr)
		{
			if (file_under_mouse->is_desktop_file())
			{
				DesktopFile *df = DesktopFile::FromPath(file_under_mouse_full_path,
					app_->possible_categories(), app_->env());
				if (df != nullptr)
				{
					{
						QAction *action = menu->addAction(tr("Run as a Program"));
						connect(action, &QAction::triggered, [=] {
							app_->LaunchOrOpenDesktopFile(file_under_mouse_full_path,
								false, RunAction::Run);
						});
						action->setIcon(df->CreateQIcon());
					}
					{
						QAction *action = menu->addAction(tr("Open For Editing"));
						connect(action, &QAction::triggered, [=] {
							app_->LaunchOrOpenDesktopFile(file_under_mouse_full_path,
								false, RunAction::Open);
						});
						action->setIcon(df->CreateQIcon());
					}
					delete df;
				}
			}
			AddOpenWithMenuTo(menu, file_under_mouse_full_path);
		}
	}
	
	QMenu *new_menu = app_->CreateNewMenu();
	menu->addMenu(new_menu);
	
	if (file_under_mouse_full_path.isEmpty())
	{
		{
			menu->addSeparator();
			QAction *action = menu->addAction(tr("New Tab"));
			action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
			connect(action, &QAction::triggered, [=] {
				app_->OpenNewTab();
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("window-new")));
		}
	}
	
	if (selected_count > 0)
	{
		// cut copy
		menu->addSeparator();
		QAction *action = menu->addAction(tr("Cut"));
		action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
		connect(action, &QAction::triggered, [=] {
			ActionCut();
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-cut")));
		
		action = menu->addAction(tr("Copy"));
		action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
		connect(action, &QAction::triggered, [=] {
			ActionCopy();
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
		
		action = menu->addAction(tr("Copy Names"));
		connect(action, &QAction::triggered, [=] { ActionCopyPaths(Path::OnlyName); });
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
		
		action = menu->addAction(tr("Copy Paths"));
		//action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
		connect(action, &QAction::triggered, [=] { ActionCopyPaths(Path::Full); });
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
		
		action = menu->addAction(tr("Play in MPV"));
		connect(action, &QAction::triggered, [=] { ActionPlayInMpv(); });
		action->setIcon(QIcon::fromTheme(QLatin1String("media-playback-start")));
	}
	
	QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimedata = clipboard->mimeData();
	if (mimedata->hasUrls())
	{
		QList<QUrl> urls = mimedata->urls();
		QString file_count_str = QLatin1String(" (")
			+ QString::number(urls.count()) + ')';
		{ // paste
			QAction *action = menu->addAction(tr("Paste") + file_count_str);
			action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
			connect(action, &QAction::triggered, [this] {
				ActionPaste();
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-paste")));
		}
		{
			QAction *action = menu->addAction(tr("Paste Link") + file_count_str);
			connect(action, &QAction::triggered, [this] {
				ActionPasteLinks(LinkType::Absolute);
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-paste")));
		}
		{
			QAction *action = menu->addAction(tr("Paste Relative Link") + file_count_str);
			connect(action, &QAction::triggered, [this] {
				ActionPasteLinks(LinkType::Relative);
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-paste")));
		}
		menu->addSeparator();
	}
	
	const QIcon trash_icon = QIcon::fromTheme(QLatin1String("user-trash"));
	
	if (selected_count > 0) {
		{
			QAction *action = menu->addAction(tr("Move to trash"));
			action->setShortcut(QKeySequence(Qt::Key_Delete));
			connect(action, &QAction::triggered, [=] {
				DeleteSelectedFiles(ShiftPressed::No);
			});
			action->setIcon(trash_icon);
		}
		{
			menu->addSeparator();
			QAction *action = menu->addAction(tr("Delete Permanently"));
			action->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
			connect(action, &QAction::triggered, [=] {ProcessAction(actions::DeleteFiles);});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
			menu->addSeparator();
		}
		
		{
			QAction *action = menu->addAction(tr("Rename File"));
			action->setShortcut(QKeySequence(Qt::Key_F2));
			connect(action, &QAction::triggered, [=] {ProcessAction(actions::RenameFile);});
			action->setIcon(QIcon::fromTheme(QLatin1String("insert-text")));
		}
		
		{
			QVector<QString> videos = { QLatin1String("mkv"), QLatin1String("webm") };
			
			if (videos.contains(file_under_mouse->cache().ext)) {
				QAction *action = menu->addAction(tr("Edit movie title") + QLatin1String(" [mkvpropedit]"));
				connect(action, &QAction::triggered, [=] { app_->EditSelectedMovieTitle(); });
				QIcon *icon = app_->GetIcon(file_under_mouse->cache().ext);
				if (icon != nullptr) {
					action->setIcon(*icon);
				}
			}
		}

		menu->addSeparator();
		{
			QAction *action = menu->addAction(tr("Run Executable"));
			connect(action, &QAction::triggered, [=] {ProcessAction(actions::RunExecutable);});
			action->setIcon(QIcon::fromTheme(QLatin1String("system-run")));
		}
		{
			QAction *action = menu->addAction(tr("Toggle Exec Bit"));
			connect(action, &QAction::triggered, [=] {
				app_->ToggleExecBitOfSelectedFiles();
			});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-undo")));
		}
		menu->addSeparator();
	}
	
	{
		QAction *action = menu->addAction(tr("Open Terminal"));
		action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
		connect(action, &QAction::triggered, [=] {ProcessAction(actions::OpenTerminal);});
		action->setIcon(QIcon::fromTheme(QLatin1String("utilities-terminal")));
	}
	
	if (AnyArchive(extensions)) {
		QMenu *extract = new QMenu(tr("Extract"));
		menu->addSeparator();
		menu->addMenu(extract);
		QIcon *icon = app_->GetIcon(QLatin1String("zip"));
		if (icon != nullptr)
			extract->setIcon(*icon);
		menu->addSeparator();
		
		{
			QAction *action = extract->addAction(tr("Here"));
			connect(action, &QAction::triggered, [=] {
				app_->ExtractTo(app_->tab()->current_dir());
			});
		}
		
		{
			QAction *action = extract->addAction(tr("To..."));
			connect(action, &QAction::triggered, [=] { app_->ExtractAskDestFolder(); });
		}
	}
	
	if (selected_count > 0) {
		QMenu *archive_menu = new QMenu(tr("Archive As"));
		menu->addSeparator();
		menu->addMenu(archive_menu);
		QIcon *icon = app_->GetIcon(QLatin1String("zip"));
		if (icon != nullptr)
			archive_menu->setIcon(*icon);
		menu->addSeparator();
		
		QStringList exts = {
			QLatin1String("tar.gz"), QLatin1String("zip"),
			QLatin1String("tar.xz"), QLatin1String("tar.zst"),
			QLatin1String("7z")
		};
		
		for (const auto &ext: exts)
		{
			QAction *action = archive_menu->addAction(ext);
			connect(action, &QAction::triggered, [=] {
				app_->ArchiveTo(app_->tab()->current_dir(), ext);
			});
		}
	}
	
	if (selected_count > 0) {
		QMenu *efa_menu = new QMenu(tr("Extended File Attributes"));
		menu->addSeparator();
		menu->addMenu(efa_menu);
		
		QIcon *icon = app_->GetIcon(QLatin1String("php"));
		if (icon) {
			efa_menu->setIcon(*icon);
		}
		
		menu->addSeparator();
		
		{
			QAction *action = efa_menu->addAction("Remove Thumbnails");
			connect(action, &QAction::triggered, [=] {
				RemoveEfaFromSelectedFiles(Efa::Thumbnail);
			});
		}
		
		{
			QAction *action = efa_menu->addAction("Allow Thumbnails");
			connect(action, &QAction::triggered, [=] {
				AllowEfaInSelectedFiles(Efa::Thumbnail);
			});
		}
		
		{
			QAction *action = efa_menu->addAction("Remove All Text EFA");
			connect(action, &QAction::triggered, [=] {
				RemoveEfaFromSelectedFiles(Efa::Text);
			});
		}
		
		{
			QAction *action = efa_menu->addAction("Allow All Text EFA");
			connect(action, &QAction::triggered, [=] {
				AllowEfaInSelectedFiles(Efa::Text);
			});
		}
		
		{
			QAction *action = efa_menu->addAction("Remove All EFA");
			connect(action, &QAction::triggered, [=] {
				RemoveEfaFromSelectedFiles(Efa::All);
			});
		}
		
		{
			QAction *action = efa_menu->addAction("Allow All EFA");
			connect(action, &QAction::triggered, [=] {
				AllowEfaInSelectedFiles(Efa::All);
			});
		}
	}
	
	if (selected_count == 0)
	{
		QString current_trash_dir = CurrentDirTrashPath();
		if (io::DirExists(current_trash_dir)){
			menu->addSeparator();
			if (undo_delete_menu_ == nullptr)
			{
				undo_delete_menu_ = new QMenu(tr("Undo Delete"));
				undo_delete_menu_->setIcon(QIcon::fromTheme(QLatin1String("edit-undo")));
				connect(undo_delete_menu_, &QMenu::aboutToShow, [=]() {
					PopulateUndoDelete(undo_delete_menu_);
				});
			}
			menu->addMenu(undo_delete_menu_);
		}
		
		{
			QAction *action = menu->addAction(tr("Empty trash"));
			connect(action, &QAction::triggered, [=]
			{
				QMessageBox::StandardButton reply = QMessageBox::question(this,
					tr("Please confirm"),
					tr("Empty trash can?"), QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);
				
				if (reply == QMessageBox::Yes)
					trash::EmptyRecursively(current_dir);
			});
			action->setIcon(trash_icon);
		}
	}
	
	QString count_folder = dir_full_path.isEmpty() ? current_dir : dir_full_path;
	
	if (count_folder != QLatin1String("/")) {
		menu->addSeparator();
		QAction *action = menu->addAction(tr("Folder stats"));
		
		connect(action, &QAction::triggered, [=] {
			gui::CountFolder cf(app_, count_folder);
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("emblem-important")));
	}
	
	UpdateView();
	menu->popup(global_pos);
}

void Tab::ShutdownLastInotifyThread()
{
#ifdef CORNUS_WAITED_FOR_WIDGETS
	using Clock = std::chrono::steady_clock;
	auto start_time = Clock::now();
#endif
	{
		io::Files &files = view_files();
		auto g = files.guard();
		files.data.thread_must_exit(true);
		files.WakeUpInotify(Lock::No);
		
		while (!files.data.thread_exited())
		{
			files.CondWait();
		}
	}
#ifdef CORNUS_WAITED_FOR_WIDGETS
	auto now = Clock::now();
	const float elapsed = std::chrono::duration<float,
		std::chrono::milliseconds::period>(now - start_time).count();
	mtl_info("Waited for thread termination: %.1f ms", elapsed);
#endif
}

void Tab::StartDragOperation()
{
	auto &files = view_files();
	QMimeData *mimedata = new QMimeData();
	QList<QUrl> urls;
	QPair<int, int> files_folders = files.ListSelectedFiles(Lock::Yes, urls);
	mtl_check_void(!urls.isEmpty());
	mimedata->setUrls(urls);
	
/// Set a pixmap that will be shown alongside the cursor during the operation:
	
	cint img_w = 128;
	cint img_h = img_w / 2;
	QPixmap pixmap(QSize(img_w, img_h));
	QPainter painter(&pixmap);
	
	QRect r(0, 0, img_w, img_h);
	painter.fillRect(r, QColor(235, 235, 255));
	
	QPen pen(QColor(0, 0, 0));
	painter.setPen(pen);
	
	QString dir_str = QString::number(files_folders.first)
		+ QString(" folder(s)");
	QString file_str = QString::number(files_folders.second)
		+ QString(" file(s)");
	auto str_rect = fontMetrics().boundingRect(dir_str);
	
	auto r2 = r;
	r2.setY(r2.y() + str_rect.height());
	
	painter.drawText(r, Qt::AlignCenter | Qt::AlignVCenter, dir_str, &r);
	painter.drawText(r2, Qt::AlignCenter | Qt::AlignVCenter, file_str, &r2);

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimedata);
	drag->setPixmap(pixmap);
	{
		/** Warning! changing this to:
		 drag->exec(Qt::CopyAction | Qt::MoveAction);
		 will break dragging movie files onto the MPV player. */
		drag->exec(Qt::CopyAction);
	}
}

void Tab::TellEfaOfSelectedFile() {
	auto &files = view_files();
	io::File *cloned_file = nullptr;
	cint row = files.GetFirstSelectedFile(Lock::Yes, &cloned_file, Clone::Yes);
	if (row == -1)
		return;
	AutoDelete ad(cloned_file);
	const Efa efa = app_->blacklist().GetStatus(cloned_file);
	QString s = cloned_file->name();
	if (EfaContains(efa, Efa::Text)) {
		s.append("\n \u274C Text not allowed");
	}
	
	if (EfaContains(efa, Efa::Thumbnail)) {
		s.append("\n \u274C Thumbnails not allowed");
	}
	
	QMessageBox msgBox;
	msgBox.setText(s);
	msgBox.exec();
}

void Tab::ToggleMagnifiedMode()
{
	magnified(!magnified());
	UpdateView();
}

void Tab::UndeleteFiles(const QMap<i64, QVector<trash::Names>> &items)
{
	QVector<QString> filenames;
	auto &files = view_files();
	{
		auto g = files.guard();
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
		view_files().SelectFilenamesLater(select_vec, SameDir::Yes);
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

void Tab::UpdateIndices(const QSet<int> &indices)
{
	if (indices.isEmpty())
		return;
	
	switch (view_mode_)
	{
	case ViewMode::Details : {
		table_model_->UpdateIndices(indices);
		break;
	}
	case ViewMode::Icons: {
		icon_view_->UpdateIndices(indices);
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

void Tab::UpdateView()
{
	switch (view_mode_)
	{
	case ViewMode::Details : {
		table_model_->UpdateVisibleArea();
		break;
	}
	case ViewMode::Icons: {
		icon_view_->UpdateVisibleArea();
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

io::Files& Tab::view_files() const { return *app_->files(files_id_); }

bool Tab::ViewIsAt(const QString &dir_path) const
{
	QString old_dir_path;
	auto &files = view_files();
	{
		auto g = files.guard();
		old_dir_path = files.data.processed_dir_path;
	}
	
	if (old_dir_path.isEmpty())
		return false;
	
	return io::SameFiles(dir_path, old_dir_path);
}

QScrollBar* Tab::ViewScrollBar() const
{
	switch (view_mode_)
	{
	case ViewMode::Details: return table_->scrollbar();
	case ViewMode::Icons: return icon_view_->scrollbar();
	default: return nullptr;
	}
}

int Tab::ViewRowHeight() const
{
	switch (view_mode_)
	{
	case ViewMode::Details: return table_->GetRowHeight();
	case ViewMode::Icons: return icon_view_->GetRowHeight();
	default: { mtl_trace(); return -1; }
	}
}

}} // cornus::gui
