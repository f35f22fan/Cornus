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
#include "Location.hpp"
#include "OpenOrderPane.hpp"
#include "../str.hxx"
#include "Table.hpp"
#include "TableModel.hpp"
#include "TabsWidget.hpp"
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
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QUrl>

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

	if (!params->reload) {
		if (tab->ViewIsAt(params->dir_path.path)) {
			delete params;
			return nullptr;
		}
	}

	if (!params->dir_path.path.endsWith('/'))
		params->dir_path.path.append('/');
	
	io::FilesData *new_data = new io::FilesData();
	new_data->reloaded(params->reload);
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

	const auto cdf = app->prefs().show_dir_file_count() ?
		io::CountDirFiles::Yes : io::CountDirFiles::No;
	
	if (io::ListFiles(*new_data, &view_files, cdf) != 0) {
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

bool SendURLsClipboard(const QStringList &list, io::Message msg_id) {
	auto ba = new ByteArray();
	ba->set_msg_id(msg_id);
	for (const auto &next: list)
	{
		ba->add_string(next);
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
	QStringList list;
	VOID_RET_IF(CreateMimeWithSelectedFiles(ClipboardAction::Copy, list), false);
	VOID_RET_IF(SendURLsClipboard(list, io::Message::CopyToClipboard), false);
}

void Tab::ActionCut()
{
	QStringList list;
	if (!CreateMimeWithSelectedFiles(ClipboardAction::Cut, list))
		return;
	
	SendURLsClipboard(list, io::Message::CutToClipboard);
}

void Tab::ActionPaste()
{
	const Clipboard &clipboard = app_->clipboard();
	
	io::Message io_op = io::Message::None;
	if (clipboard.action == ClipboardAction::Copy) {
		io_op = io::Message::Copy;
		io_op |= io::Message::DontTryAtomicMove;
	} else {
		io_op = io::Message::Move;
	}
	
	QString to_dir = app_->tab()->current_dir();
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		VOID_RET_IF(hash_info.valid(), false);
	}
	
	auto *ba = new ByteArray();
	if (needs_root)
	{
		ba->add_u64(hash_info.num);
	}
	ba->set_msg_id(io_op);
	ba->add_string(to_dir);
	QVector<QString> names;
	for (const auto &next: clipboard.file_paths)
	{
		QString filename = io::GetFileNameOfFullPath(next).toString();
		names.append(filename);
		ba->add_string(next);
	}
	
	view_files().SelectFilenamesLater(names, SameDir::Yes);
	io::socket::SendAsync(ba, socket_path);
	if (clipboard.action == ClipboardAction::Cut)
	{
		/// Not using qclipboard->clear() because it doesn't work:
		QApplication::clipboard()->setMimeData(new QMimeData());
	}
}

void Tab::ActionPasteLinks(const LinkType link)
{
	const Clipboard &clipboard = app_->clipboard();
	QString err;
	QVector<QString> names;
	QString to_dir_path = app_->tab()->current_dir();
	
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir_path, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		VOID_RET_IF(hash_info.valid(), false);
		
		auto *ba = new ByteArray();
		ba->add_u64(hash_info.num);
		const auto msg = (link == LinkType::Absolute) ? io::Message::PasteLinks
			: io::Message::PasteRelativeLinks;
		ba->set_msg_id(msg);
		ba->add_string(to_dir_path);
		
		for (const auto &next: clipboard.file_paths)
		{
			ba->add_string(next);
			QStringRef s = io::GetFileNameOfFullPath(next);
			
			if (!s.isEmpty())
				names.append(s.toString());
		}
		
		io::socket::SendAsync(ba, socket_path);
	} else {
		if (link == LinkType::Absolute)
			io::PasteLinks(clipboard.file_paths, to_dir_path, &names, &err);
		else if (link == LinkType::Relative)
			io::PasteRelativeLinks(clipboard.file_paths, to_dir_path, &names, &err);
		else {
			mtl_trace();
			return;
		}
		
		if (clipboard.action == ClipboardAction::Cut)
		{
			/// Not using qclipboard->clear() because it doesn't work:
			QApplication::clipboard()->setMimeData(new QMimeData());
		}
		
		if (!err.isEmpty()) {
			app_->TellUser(tr("Paste Link Error: ") + err);
		}
	}
	
	view_files().SelectFilenamesLater(names, SameDir::Yes);
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

bool Tab::CreateMimeWithSelectedFiles(const ClipboardAction action,
	QStringList &list)
{
	auto &files = view_files();
	MutexGuard guard = files.guard();
	
	io::FileBits flag = io::FileBits::Empty;
	if (action == ClipboardAction::Cut)
		flag = io::FileBits::ActionCut;
	else if (action == ClipboardAction::Copy)
		flag = io::FileBits::ActionCopy;
	else if (action == ClipboardAction::Paste)
		flag = io::FileBits::ActionPaste;
	else if (action == ClipboardAction::Link)
		flag = io::FileBits::PasteLink;
	
	QSet<int> indices;
	int i = -1;
	for (io::File *next: files.data.vec)
	{
		i++;
		
		if (next->is_selected()) {
			indices.insert(i);
			next->toggle_flag(flag, true);
			QString s = next->build_full_path();
			list.append(QUrl::fromLocalFile(s).toString());
		} else {
			if (next->clear_all_actions_if_needed())
				indices.insert(i);
		}
	}
	
	UpdateIndices(indices);
	return true;
}

QVector<QAction*>
Tab::CreateOpenWithList(const QString &full_path)
{
	open_with_.full_path = full_path;
	open_with_.mime = app_->QueryMimeType(full_path);
	ReloadOpenWith();
	QVector<QAction*> ret;
	
	for (DesktopFile *next: open_with_.show_vec)
	{
		QString name = next->GetName();
		QString generic = next->GetGenericName();
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
		
		for (io::File *next: files.data.vec) {
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
		hash_info = app_->WaitForRootDaemon(CanOverwrite::No);
		VOID_RET_IF(hash_info.valid(), false);
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
			if (table_->GetFileAt_NoLock(evt->pos(), PickBy::VisibleName, &to_dir) != -1
				&& to_dir->is_dir_or_so())
			{
				to_dir = to_dir->Clone();
			} else {
				to_dir = io::FileFromPath(current_dir());
			}
		} else if (view_mode_ == ViewMode::Icons) {
			to_dir = icon_view_->GetFileAt_NoLock(evt->pos(), Clone::No);
		} else {
			/// Otherwise drop onto current directory:
			to_dir = io::FileFromPath(current_dir());
		}
	}
	
	VOID_RET_IF(to_dir, nullptr);
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
	VOID_RET_IF(files_vec, nullptr);
	const QString to_dir_path = to_dir->build_full_path();
	bool needs_root;
	const char *socket_path = io::QuerySocketFor(to_dir_path, needs_root);
	HashInfo hash_info;
	if (needs_root)
	{
		hash_info = app_->WaitForRootDaemon(CanOverwrite::Yes);
		VOID_RET_IF(hash_info.valid(), false);
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

void Tab::FilesChanged(const FileCountChanged fcc, const int row)
{
	// File changes are monitored only in TableModel.cpp which represents
	// the detailed view, which is why the detailed view uses
	// this method to inform the icon view of changes.
	auto *iv = icon_view();
	if (iv != nullptr)
	{
		iv->FilesChanged(fcc, row);
	}
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
		file_index = table_->GetFileAt_NoLock(local_pos, PickBy::VisibleName, &file);
	} else {
		file = icon_view_->GetFileAt_NoLock(local_pos, Clone::Yes, &file_index);
	}
	
	if (file_index != -1 && ret_cloned_file != nullptr)
		*ret_cloned_file = file->Clone();
	
	if (full_path != nullptr)
		*full_path = file->build_full_path();

	return file_index;
}

void Tab::GetSelectedArchives(QVector<QString> &urls)
{
	io::Files &files = view_files();
	MutexGuard guard = files.guard();
	
	for (io::File *next: files.data.vec) {
		if (!next->is_selected() || !next->is_regular())
			continue;
		
		QString ext = next->cache().ext.toString();
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
		io::Files &files = view_files();
		got_files_to_select = !files.data.filenames_to_select.isEmpty();
	}
	table_model_->SwitchTo(new_data);
	history_->Add(new_data->action, current_dir_);
	
	if (new_data->action == Action::Back) {
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
	
	FigureOutSelectPath(cmds.select, cmds.go_to_path);
	if (!cmds.go_to_path.isEmpty())
	{
		io::FileType file_type;
		if (!io::FileExists(cmds.go_to_path, &file_type)) {
			QString name = cmds.select;
			if (name.startsWith('/'))
				name = io::GetFileNameOfFullPath(name).toString();
			view_files().SelectFilenamesLater({name});
			GoTo(Action::To, {QDir::homePath(), Processed::No}, Reload::No);
			return;
		}
		if (file_type == io::FileType::Dir) {
			QString name = cmds.select;
			if (name.startsWith('/'))
				name = io::GetFileNameOfFullPath(name).toString();
			view_files().SelectFilenamesLater({name});
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
			view_files().SelectFilenamesLater({name});
			GoTo(Action::To, {parent_dir.absolutePath(), Processed::No}, Reload::No);
			return;
		}
	}
}

void Tab::GoToAndSelect(const QString full_path)
{
	QFileInfo info(full_path);
	QDir parent = info.dir();
	VOID_RET_IF(parent.exists(), false);
	QString parent_dir = parent.absolutePath();
	const QString name = io::GetFileNameOfFullPath(full_path).toString();
	const SameDir same_dir = io::SameFiles(parent_dir, current_dir_) ? SameDir::Yes : SameDir::No;
	view_files().SelectFilenamesLater({name}, same_dir);
	
	if (same_dir == SameDir::No) {
		VOID_RET_IF(GoTo(Action::To, {parent_dir, Processed::No}, Reload::No), false);
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
	view_files().SelectFilenamesLater({name});
	GoTo(Action::Up, {dir.absolutePath(), Processed::Yes}, Reload::No);
}

void Tab::GrabFocus() {
	table_->setFocus();
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
		file_index = table_->GetFileAt_NoLock(pos, PickBy::VisibleName, &file);
		shift_select = table_->shift_select();
	} else if (view_mode_ == ViewMode::Icons) {
		file = icon_view_->GetFileAt_NoLock(pos, Clone::No, &file_index);
		shift_select = icon_view_->shift_select();
	} else {
		mtl_tbd();
	}
	
	if (file == nullptr) {
		files.SelectAllFiles_NoLock(Selected::No, indices);
	} else {
		if (!file->is_selected()) {
			files.SelectAllFiles_NoLock(Selected::No, indices);
			file->set_selected(true);
			if (shift_select)
				shift_select->base_row = file_index;
			indices.insert(file_index);
		}
	}
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
	const int key = evt->key();
	auto *app = app_;
	const auto modifiers = evt->modifiers();
	const bool any_modifiers = (modifiers != Qt::NoModifier);
	const bool shift = (modifiers & Qt::ShiftModifier);
	const bool ctrl = (modifiers & Qt::ControlModifier);
	QSet<int> indices;
	
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
		
		if (shift) {
			if (key == Qt::Key_U) {
				view_files().RemoveThumbnailsFromSelectedFiles();
			}
		}
	}
	
	if (key == Qt::Key_Delete) {
		DeleteSelectedFiles(shift ? ShiftPressed::Yes : ShiftPressed::No);
	} else if (key == Qt::Key_F2) {
		app_->RenameSelectedFile();
	} else if (key == Qt::Key_Return) {
		if (!any_modifiers) {
			io::File *cloned_file = nullptr;
			int row = files.GetFirstSelectedFile_Lock(&cloned_file);
			if (row != -1) {
				app->FileDoubleClicked(cloned_file, PickBy::VisibleName);
				indices.insert(row);
			}
		}
	} else if (key == Qt::Key_Down || key == Qt::Key_Right) {
		if (shift)
			app_->hid()->HandleKeyShiftSelect(this, VDirection::Down, key);
		else
			app_->hid()->HandleKeySelect(this, VDirection::Down, key);
	} else if (key == Qt::Key_Up || key == Qt::Key_Left) {
		if (shift)
			app_->hid()->HandleKeyShiftSelect(this, VDirection::Up, key);
		else
			app_->hid()->HandleKeySelect(this, VDirection::Up, key);
	} else if (key == Qt::Key_D) {
		if (any_modifiers)
			return;
		io::File *cloned_file = nullptr;
		int row = files.GetFirstSelectedFile_Lock(&cloned_file);
		if (row != -1)
			app_->DisplayFileContents(row, cloned_file);
		else {
			app_->HideTextEditor();
		}
	} else if (key == Qt::Key_F) {
		if (!any_modifiers)
			SetNextView();
	} else if (key == Qt::Key_Escape) {
		app_->HideTextEditor();
	} else if (key == Qt::Key_PageUp) {
		auto *vs = ViewScrollBar();
		VOID_RET_IF(vs, nullptr);
		const int rh = ViewRowHeight();
		const int page = vs->pageStep() - rh * 2;
		const int new_val = vs->value() - page;
		vs->setValue(new_val);
		int r = new_val / rh;
		if (new_val % rh != 0)
			r++;
		int row = std::max(0, r);
		app_->hid()->SelectFileByIndex(this, row, DeselectOthers::Yes);
	} else if (key == Qt::Key_PageDown) {
		auto *vs = ViewScrollBar();
		VOID_RET_IF(vs, nullptr);
		const int rh = ViewRowHeight();
		const int page = vs->pageStep() - rh;
		const int new_val = vs->value() + page;
		vs->setValue(new_val);
		int new_val2 = new_val + page;
		const int file_count = files.cached_files_count;
		int row = std::min(file_count - 1, new_val2 / rh);
		app_->hid()->SelectFileByIndex(this, row, DeselectOthers::Yes);
	} else if (key == Qt::Key_Home) {
		QScrollBar *vs = ViewScrollBar();
		VOID_RET_IF(vs, nullptr);
		vs->setValue(0);
		app_->hid()->SelectFileByIndex(this, 0, DeselectOthers::Yes);
	} else if (key == Qt::Key_End) {
		auto *vs = ViewScrollBar();
		VOID_RET_IF(vs, nullptr);
		vs->setValue(vs->maximum());
		const auto count = view_files().cached_files_count;
		app_->hid()->SelectFileByIndex(this, count - 1, DeselectOthers::Yes);
	} else if (key == Qt::Key_M && !any_modifiers) {
		io::File *cloned_file = nullptr;
		files.GetFirstSelectedFile_Lock(&cloned_file);
		if (cloned_file != nullptr)
			AttrsDialog attrs_d(app_, cloned_file);
	}
	
	UpdateIndices(indices);
}

void Tab::LaunchFromOpenWithMenu()
{
	QAction *act = qobject_cast<QAction *>(sender());
	QVariant v = act->data();
	DesktopFile *p = (DesktopFile*) v.value<void *>();
	p->Launch(open_with_.full_path, current_dir());
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

void Tab::PopulateUndoDelete(QMenu *menu)
{
	menu->clear();
	QMap<i64, QVector<trash::Names>> all_items;
	VOID_RET_IF(trash::ListItems(CurrentDirTrashPath(), all_items), false);
	
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

void Tab::ProcessAction(const QString &action)
{
	App *app = app_;
	auto &files = this->view_files();
	
	if (action == actions::DeleteFiles) {
		int count = files.GetSelectedFilesCount_Lock();
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
		QString full_path = files.GetFirstSelectedFileFullPath_Lock(&ext);
		if (!full_path.isEmpty()) {
			ExecInfo info = app->QueryExecInfo(full_path, ext);
			if (info.is_elf() || info.is_shell_script())
				app->RunExecutable(full_path, info);
		}
	} else if (action == actions::ToggleExecBit) {
		app->ToggleExecBitOfSelectedFiles();
	} else if (action == actions::EditMovieTitle) {
		app->EditSelectedMovieTitle();
	}
}

bool Tab::ReloadOpenWith()
{
	open_with_.Clear();
	int fd = io::socket::Client(cornus::SocketPath);
	if (fd == -1)
		return false;
	
	ByteArray send_ba;
	send_ba.set_msg_id(io::Message::SendOpenWithList);
	send_ba.add_string(open_with_.mime);
	
	if (!send_ba.Send(fd, CloseSocket::No))
		return false;
	
	ByteArray receive_ba;
	if (!receive_ba.Receive(fd))
		return false;
	
	while (receive_ba.has_more())
	{
		const Present present = (Present)receive_ba.next_i8();
		DesktopFile *next = DesktopFile::From(receive_ba);
		if (next != nullptr) {
			if (present == Present::Yes)
				open_with_.show_vec.append(next);
			else
				open_with_.hide_vec.append(next);
		}
	}
	
	return true;
}

void Tab::resizeEvent(QResizeEvent *ev)
{
	QWidget::resizeEvent(ev);
}

void Tab::ScrollToFile(const int file_index)
{
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

void Tab::SetScrollValue(const int n)
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
		VOID_RET_IF(table_, nullptr);
		table_->setFocus(Qt::MouseFocusReason);
		break;
	}
	case ViewMode::Icons: {
		if (icon_view_ == nullptr) {
			AddIconsView();
		}
		icon_view_->SetAsCurrentView(NewState::AboutToSet);
		viewmode_stack_->setCurrentIndex(icons_view_index_);
		icon_view_->setFocus(Qt::MouseFocusReason);
		icon_view_->SetAsCurrentView(NewState::Set);
		break;
	}
	default: {
		mtl_trace();
	}
	}
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
	const int selected_count = files.GetSelectedFilesCount_Lock(&extensions);
	QMenu *menu = new QMenu();
	menu->setAttribute(Qt::WA_DeleteOnClose);
	
	const QString current_dir = this->current_dir();
	QString dir_full_path;
	QString file_under_mouse_full_path;
	io::File *file = nullptr;
	cornus::AutoDelete ad(file);
	{
		if (GetFileUnderMouse(local_pos, &file) != -1) {
			file_under_mouse_full_path = file->build_full_path();
			if (file->is_dir()) {
				dir_full_path = file_under_mouse_full_path;
			}
		}
	}
	
	if (selected_count == 1 && !file_under_mouse_full_path.isEmpty()) {
		if (file->cache().ext == str::Desktop)
		{
			DesktopFile *df = DesktopFile::FromPath(file_under_mouse_full_path,
				app_->possible_categories());
			if (df != nullptr)
			{
				{
					QAction *action = menu->addAction(tr("Run"));
					connect(action, &QAction::triggered, [=] {
						app_->LaunchOrOpenDesktopFile(file_under_mouse_full_path,
							false, RunAction::Run);
					});
					action->setIcon(df->CreateQIcon());
				}
				{
					QAction *action = menu->addAction(tr("Open"));
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
	
	QMenu *new_menu = app_->CreateNewMenu();
	menu->addMenu(new_menu);
	
	if (selected_count == 0) {
		{
			menu->addSeparator();
			QAction *action = menu->addAction(tr("New Tab"));
			action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
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
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_X));
		connect(action, &QAction::triggered, [=] {
			ActionCut();
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-cut")));
		
		action = menu->addAction(tr("Copy"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));
		connect(action, &QAction::triggered, [=] {
			ActionCopy();
		});
		action->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
	}
	
	const Clipboard &clipboard = app_->clipboard();
	if (clipboard.has_files())
	{
		QString file_count_str = QLatin1String(" (")
			+ QString::number(clipboard.file_count()) + ')';
		{ // paste
			QAction *action = menu->addAction(tr("Paste") + file_count_str);
			action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_V));
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
			action->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete));
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
			
			if (videos.contains(file->cache().ext.toString())) {
				QAction *action = menu->addAction(tr("Edit movie title") + QLatin1String(" [mkvpropedit]"));
				connect(action, &QAction::triggered, [=] {ProcessAction(actions::EditMovieTitle);});
				QIcon *icon = app_->GetIcon(file->cache().ext.toString());
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
			action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
			connect(action, &QAction::triggered, [=] {ProcessAction(actions::ToggleExecBit);});
			action->setIcon(QIcon::fromTheme(QLatin1String("edit-undo")));
		}
		menu->addSeparator();
	}
	
	{
		QAction *action = menu->addAction(tr("Open Terminal"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
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
	io::Files *p = app_->files(files_id_);
	VOID_RET_IF(p, nullptr);
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

void Tab::StartDragOperation()
{
	auto &files = view_files();
	QMimeData *mimedata = new QMimeData();
	QList<QUrl> urls;
	QPair<int, int> files_folders = files.ListSelectedFiles_Lock(urls);
	if (urls.isEmpty())
		return;
	
	mimedata->setUrls(urls);
	
/// Set a pixmap that will be shown alongside the cursor during the operation:
	
	const int img_w = 128;
	const int img_h = img_w / 2;
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
	
	painter.drawText(r, Qt::AlignCenter + Qt::AlignVCenter, dir_str, &r);
	painter.drawText(r2, Qt::AlignCenter + Qt::AlignVCenter, file_str, &r2);

	QDrag *drag = new QDrag(this);
	drag->setMimeData(mimedata);
	drag->setPixmap(pixmap);
	{
		/** Warning: changing this to:
		 drag->exec(Qt::CopyAction | Qt::MoveAction);
		 will break dragging movie files onto the MPV player. */
		drag->exec(Qt::CopyAction);
	}
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
