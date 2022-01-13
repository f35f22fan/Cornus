#include "TableModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/File.hpp"
#include "../io/Notify.hpp"
#include "Location.hpp"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "Tab.hpp"
#include "Table.hpp"
#include "TableHeader.hpp"
#include "../io/uring.hh"

#include <liburing.h>
#include <sys/epoll.h>
#include <QFont>
#include <QScrollBar>
#include <QTime>
#include <QTimer>

//#define CORNUS_DEBUG_INOTIFY_BATCH
//#define CORNUS_DEBUG_INOTIFY

namespace cornus::gui {

const auto ConnectionType = Qt::BlockingQueuedConnection;
const size_t kInotifyEventBufLen = 16 * (sizeof(struct inotify_event) + NAME_MAX + 1);

struct WatchArgs {
	i32 dir_id = 0;
	QString dir_path;
	cornus::gui::TableModel *table_model = nullptr;
};

// helper struct to deal with inotify's shitty rename approach.
struct RenameData {
	QString name;
	u32 cookie = 0;
	u8 checked_times = 0;
};

io::File* Find(const QVector<io::File*> &vec,
	const QString &name, int *index)
{
	int i = -1;
	for (io::File *file : vec)
	{
		i++;
		if (file->name() == name) {
			if (index != nullptr)
				*index = i;
			return file;
		}
	}
	
	return nullptr;
}

int FindPlace(io::File *new_file, QVector<io::File*> &files_vec)
{
	for (int i = files_vec.size() - 1; i >= 0; i--) {
		io::File *next = files_vec[i];
		if (!io::SortFiles(new_file, next)) {
			return i + 1;
		}
	}
	
	return 0;
}

int FindTheOtherNameByCookie(QVector<RenameData> &vec, const u32 cookie,
	QString &result)
{
	int i = 0;
	for (const RenameData &next: vec)
	{
		if (next.cookie == cookie)
		{
			result = next.name;
			return i;
		}
		i++;
	}
	
	return -1;
}

void InsertFile(io::File *new_file, QVector<io::File*> &files_vec,
	int *inserted_at)
{
	int index = FindPlace(new_file, files_vec);
	if (inserted_at != nullptr)
		*inserted_at = index;
	
	files_vec.insert(*inserted_at, new_file);
}

void SendCreateEvent(TableModel *model, cornus::io::Files *files,
	const QString new_name, const i32 dir_id)
{
	io::File *new_file = new io::File(files);
	new_file->name(new_name);
	struct statx stx;
	if (!io::ReloadMeta(*new_file, stx))
		mtl_trace();
	
	io::FileEvent evt = {};
	evt.new_file = new_file;
	evt.dir_id = dir_id;
	evt.type = io::FileEventType::Created;
	QMetaObject::invokeMethod(model, "InotifyEvent",
		ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
}

void SendDeleteEvent(TableModel *model, cornus::io::Files *files,
	const QString name, const i32 dir_id)
{
	int index = -1;
	{
		MutexGuard guard = files->guard();
		Find(files->data.vec, name, &index);
	}
	
	VOID_RET_IF(index, -1);
	
	io::FileEvent evt = {};
	evt.dir_id = dir_id;
	evt.index = index;
	evt.type = io::FileEventType::Deleted;
	QMetaObject::invokeMethod(model, "InotifyEvent",
		ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
}

void SendModifiedEvent(TableModel *model, cornus::io::Files *files,
	const QString name, const i32 dir_id)
{
	int index = -1;
	io::File *found = nullptr;
	{
		MutexGuard guard = files->guard();
		found = Find(files->data.vec, name, &index);
		VOID_RET_IF(found, nullptr);
		struct statx stx;
		VOID_RET_IF(io::ReloadMeta(*found, stx), false);
	}
	
	io::FileEvent evt = {};
	evt.dir_id = dir_id;
	evt.index = index;
	evt.type = io::FileEventType::Modified;
	QMetaObject::invokeMethod(model, "InotifyEvent",
		ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
}

void ReadEvent(const int inotify_fd, char *buf, cornus::io::Files *files,
	bool &has_been_unmounted_or_deleted,
	const bool with_hidden_files,
	cornus::gui::TableModel *model,
	const int dir_id,
	const int wd,
	QVector<RenameData> &rename_vec)
{
#ifdef CORNUS_DEBUG_INOTIFY
	QString num_str = QString::number(i64(pthread_self()), 36);
	mtl_info("Thread %s, wd: %d, dir_id: %d", qPrintable(num_str), wd, dir_id);
#endif
	const ssize_t num_read = read(inotify_fd, buf, kInotifyEventBufLen);
	if (num_read <= 0) {
		if (num_read == -1)
			mtl_status(errno);
		return;
	}
	
	if (has_been_unmounted_or_deleted) {
		mtl_trace();
		return;
	}
	
	ssize_t add = 0;
	
	for (char *p = buf; p < buf + num_read; p += add)
	{
		struct inotify_event *ev = (struct inotify_event*) p;
		add = sizeof(struct inotify_event) + ev->len;
		if (ev->wd != wd) {
			//mtl_trace("ev->wd: %d, wd: %d", ev->wd, wd);
			continue;
		}
		const auto mask = ev->mask;
		//const bool is_dir = mask & IN_ISDIR;
		
		if (mask & (IN_ATTRIB | IN_MODIFY)) {
			QString name(ev->name);
			if (!with_hidden_files && name.startsWith('.'))
				continue;
			
			SendModifiedEvent(model, files, name, dir_id);
		} else if (mask & IN_CREATE) {
			QString name(ev->name);
			if (!with_hidden_files && name.startsWith('.'))
				continue;
			
#ifdef CORNUS_DEBUG_INOTIFY
mtl_trace("IN_CREATE: %s", ev->name);
#endif
			SendCreateEvent(model, files, name, dir_id);
		} else if (mask & IN_DELETE) {
			QString name(ev->name);
			if (!with_hidden_files && name.startsWith('.')) {
				continue;
			}
			
#ifdef CORNUS_DEBUG_INOTIFY
mtl_trace("IN_DELETE: %s", ev->name);
#endif
			SendDeleteEvent(model, files, name, dir_id);
		} else if (mask & IN_DELETE_SELF) {
			mtl_warn("IN_DELETE_SELF");
			has_been_unmounted_or_deleted = true;
			break;
		} else if (mask & IN_MOVE_SELF) {
			mtl_warn("IN_MOVE_SELF");
		} else if (mask & IN_MOVED_FROM) {
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("IN_MOVED_FROM cookie %d, %s", ev->cookie, ev->name);
#endif
			QString name(ev->name);
			if (!with_hidden_files && name.startsWith('.')) {
				continue;
			}
			
			RenameData data = {
				.name = name,
				.cookie = ev->cookie,
				.checked_times = 0,
			};
			rename_vec.append(data);
		} else if (mask & IN_MOVED_TO) {
			QString new_name(ev->name);
			if (!with_hidden_files && new_name.startsWith('.'))
				continue;
			
			QString from_name;
			int rename_data_index = FindTheOtherNameByCookie(rename_vec, ev->cookie, from_name);
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("IN_MOVED_TO cookie %d, new_name: %s, from_name: %s", ev->cookie, ev->name,
	qPrintable(from_name));
#endif
			if (rename_data_index == -1) {
				SendCreateEvent(model, files, new_name, dir_id);
			} else {
				rename_vec.remove(rename_data_index);
				int index = -1, removed_at = -1;
				int deleted_file_index = -1;
				{
					MutexGuard guard = files->guard();
					auto &vec = files->data.vec;
					io::File *file_to_delete = Find(vec, new_name, &deleted_file_index);
					
					if (file_to_delete != nullptr) {
						vec.remove(deleted_file_index);
						delete file_to_delete;
					}
					
					io::File *old_file = Find(vec, from_name, &removed_at);
					old_file->name(new_name);
					old_file->ClearCache();// to update the icon later when needed,
					// because currently it has the icon of the previous file name.
					std::sort(vec.begin(), vec.end(), cornus::io::SortFiles);
					Find(vec, new_name, &index);
				}
				io::FileEvent evt = {};
				evt.dir_id = dir_id;
				evt.renaming_deleted_file_at = deleted_file_index;
				evt.index = index;
				evt.type = io::FileEventType::Renamed;
				QMetaObject::invokeMethod(model, "InotifyEvent",
					ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
			}
		} else if (mask & IN_Q_OVERFLOW) {
			mtl_warn("IN_Q_OVERFLOW");
		} else if (mask & IN_UNMOUNT) {
			has_been_unmounted_or_deleted = true;
			break;
		} else if (mask & IN_CLOSE_WRITE) {
			QString name(ev->name);
			if (!with_hidden_files && name.startsWith('.'))
				continue;
			
#ifdef CORNUS_DEBUG_INOTIFY
			if (ev->len > 0)
				mtl_trace("IN_CLOSE: %s", ev->name);
			else
				mtl_trace("IN_CLOSE");
#endif
			SendModifiedEvent(model, files, name, dir_id);
		} else if (mask & (IN_IGNORED | IN_CLOSE_NOWRITE)) {
		} else {
			mtl_warn("Unhandled inotify event: %u", mask);
		}
	}
}

void ReinterpretRenames(QVector<RenameData> &rename_vec,
	TableModel *model, cornus::io::Files *files, const i32 dir_id)
{
	for (int i = rename_vec.size() - 1; i >= 0; i--)
	{
		RenameData &next = rename_vec[i];
		next.checked_times++;
		if (next.checked_times > 2)
		{
			// It means that the proper IN_MOVED_TO never arrived, which
			// means the file was moved to another folder,
			// not renamed in place, so it's the same as a delete event:
			SendDeleteEvent(model, files, next.name, dir_id);
			rename_vec.remove(i);
#ifdef CORNUS_DEBUG_INOTIFY
			mtl_info("Removed from rename_vec: %s", qPrintable(next.name));
#endif
		}
	}
}

void* WatchDir(void *void_args)
{
	pthread_detach(pthread_self());
	RET_IF(void_args, nullptr, nullptr);
	
	WatchArgs *args = (WatchArgs*)void_args;
	TableModel *model = args->table_model;
	Tab *tab = model->tab();
	App *app = tab->app();
	const i64 files_id = tab->files_id();
	AutoDelete ad_args(args);
	char *buf = new char[kInotifyEventBufLen];
	AutoDeleteArr ad_arr(buf);
	io::Notify &notify = tab->notify();
	const int notify_fd = notify.fd;
	
	const auto path = args->dir_path.toLocal8Bit();
	const auto event_types = IN_ATTRIB | IN_CREATE | IN_DELETE
		| IN_DELETE_SELF | IN_MOVE_SELF | IN_CLOSE_WRITE
		| IN_MOVE;// | IN_MODIFY;
	const int wd = inotify_add_watch(notify_fd, path.data(), event_types);
	
	if (wd == -1) {
		mtl_status(errno);
		return nullptr;
	}
	
	io::AutoRemoveWatch arw(notify, wd);
	const int poll_fd = epoll_create(1);
	
	if (poll_fd == -1) {
		mtl_status(errno);
		return 0;
	}
	AutoCloseFd poll_fd___(poll_fd);
	struct epoll_event poll_event = {};
	poll_event.events = EPOLLIN;
	poll_event.data.fd = notify_fd;
	
	if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, notify_fd, &poll_event)) {
		mtl_status(errno);
		return nullptr;
	}
	
	io::Files &files = *app->files(files_id);
	QVector<RenameData> rename_vec;
	bool call_event_func = false;
	
	while (true)
	{
		{
			MutexGuard guard = files.guard();
			
			if (files.data.thread_must_exit())
				break;
			
			if (args->dir_id != files.data.dir_id)
				break;
		}
		
		const int ms = (!rename_vec.isEmpty()) ? 20 : 1000;
		const int num_file_descriptors = epoll_wait(poll_fd, &poll_event, 1, ms);
		bool with_hidden_files;
		{
			MutexGuard guard = files.guard();
			with_hidden_files = files.data.show_hidden_files();
			
			if (files.data.thread_must_exit())
				break;
			
			if (args->dir_id != files.data.dir_id)
				break;
		}
		
		bool has_been_unmounted_or_deleted = false;
		if (num_file_descriptors > 0)
		{
			ReadEvent(poll_event.data.fd, buf, &files,
				has_been_unmounted_or_deleted, with_hidden_files,
				model, args->dir_id, wd, rename_vec);
		}
		
		if (!rename_vec.isEmpty())
		{
			ReinterpretRenames(rename_vec, args->table_model, &files, args->dir_id);
		}
		
		if (rename_vec.isEmpty()) {
			{
				MutexGuard guard = files.guard();
				auto &select_list = files.data.filenames_to_select;
				call_event_func = !select_list.isEmpty() && !files.data.should_skip_selecting();
			}
			
			if (call_event_func)
			{
				/* This must be dispatched as "QueuedConnection" (low priority)
				to allow the previous
				inotify events to be processed first, otherwise file selection doesn't
				get preserved because inotify events arrive at random pace. */
				QMetaObject::invokeMethod(model, "InotifyBatchFinished", Qt::QueuedConnection);
			}
			
			if (has_been_unmounted_or_deleted) {
				arw.RemoveWatch(wd);
				break;
			}
		}
	}
	
	{
		MutexGuard guard = files.guard();
		if (args->dir_id == files.data.dir_id)
		{
			files.data.thread_exited(true);
			files.Signal();
		}
	}
	
//	mtl_trace("Thread %ld exited", i64(pthread_self()));
	return nullptr;
}

TableModel::TableModel(cornus::App *app, gui::Tab *tab): app_(app), tab_(tab)
{}

TableModel::~TableModel() {}

void TableModel::DeleteSelectedFiles(const ShiftPressed sp)
{
	QVector<QString> paths;
	{
		io::Files &files = tab_->view_files();
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

QModelIndex
TableModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}

int TableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	
	return int(Column::Count);
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
	return {};
}

QString TableModel::GetName() const
{
	static const QString name = tr("Name");
	
	if (app_->prefs().show_free_partition_space())
		return cached_free_space_.isEmpty() ? name : cached_free_space_;
	
	return name;
}

QVariant TableModel::headerData(int section_i, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (orientation == Qt::Horizontal)
		{
			const Column section = static_cast<Column>(section_i);
			
			switch (section) {
			case Column::Icon: return {};
			case Column::FileName: {
				Table *table = tab_->table();
				if (!table)
					return GetName();
				
				return (table->header()->in_drag_mode()) ?
					tr("Move to parent folder") : GetName();
			}
			case Column::Size: return tr("Size");
			case Column::TimeCreated: return tr("Created");
			case Column::TimeModified: return tr("Modified");
			default: {
				mtl_trace();
				return {};
			}
			}
		}
		return QString::number(section_i + 1);
	}
	return {};
}

void TableModel::InotifyBatchFinished()
{
	QVector<int> indices;
	auto &files = tab_->view_files();
	{
		MutexGuard guard = files.guard();
		auto &select_list = files.data.filenames_to_select;
		if (select_list.isEmpty() || files.data.should_skip_selecting())
			return;
		
#ifdef CORNUS_DEBUG_INOTIFY_BATCH
mtl_info("==> New Batch ==>>");
#endif

		QVector<io::File*> &files_vec = files.data.vec;
		auto it = select_list.begin();
		const int files_count = files_vec.size();
		
		while (it != select_list.end())
		{
			const QString &name = it.key();
			bool found = false;
			for (int i = 0; i < files_count; i++)
			{
				io::File *file = files_vec[i];
				if (file->name() == name) {
					indices.append(i);
					file->set_selected(true);
//					mtl_info("Found! %s, index: %d, file_count: %d",
//						qPrintable(file->name()), i, files_count);
#ifdef CORNUS_DEBUG_INOTIFY_BATCH
					mtl_info("Selected: %s", qPrintable(name));
#endif
					it = select_list.erase(it);
					found = true;
					break;
				}
			}
			if (!found) {
#ifdef CORNUS_DEBUG_INOTIFY_BATCH
				mtl_info("Not Found %s", qPrintable(name));
#endif
				const int new_count = it.value() + 1;
				if (new_count > 5) {
					it = select_list.erase(it);
				} else {
					select_list[it.key()] = new_count;
#ifdef CORNUS_DEBUG_INOTIFY_BATCH
					mtl_info("%s is now %d", qPrintable(it.key()), new_count);
#endif
					it++;
				}
			}
		}
	}
	
	if (!indices.isEmpty()) {
		tab_->UpdateIndices(indices);
		tab_->ScrollToFile(indices[0]);
	}
}

void TableModel::InotifyEvent(cornus::io::FileEvent evt)
{
	auto &files = tab_->view_files();
	{
		MutexGuard guard = files.guard();
		if (evt.dir_id != files.data.dir_id) {
			if (evt.new_file != nullptr)
				delete evt.new_file;
			return;
		}
	}
	
	switch (evt.type)
	{
	case io::FileEventType::Modified: {
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_info("MODIFIED");
#endif
		UpdateSingleRow(evt.index);
		tab_->FilesChanged(FileCountChanged::No, evt.index);
		break;
	}
	case io::FileEventType::Created: {
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_printq2("CREATED: ", evt.new_file->name());
#endif
		int index = -1;
		{
			MutexGuard guard = files.guard();
			QVector<io::File*> &vec = files.data.vec;
			const int count = vec.size();
			for (int i = 0; i < count; i++)
			{
				io::File *next = vec[i];
				if (next->name() == evt.new_file->name())
				{
					evt.new_file->CopyBits(next);
					delete next;
					vec[i] = evt.new_file;
					evt.new_file = nullptr;
					tab_->FilesChanged(FileCountChanged::No, i);
					return;
				}
			}
			
			index = FindPlace(evt.new_file, vec);
		}
		beginInsertRows(QModelIndex(), index, index);
		{
			MutexGuard guard = files.guard();
			files.data.vec.insert(index, evt.new_file);
			evt.new_file = nullptr;
			tab_->view_files().cached_files_count = files.data.vec.size();
		}
		endInsertRows();
		tab_->FilesChanged(FileCountChanged::Yes, index);
		break;
	}
	case io::FileEventType::Deleted: {
		beginRemoveRows(QModelIndex(), evt.index, evt.index);
		{
			MutexGuard guard = files.guard();
			io::File *file_to_delete = files.data.vec[evt.index];
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_printq2("DELETED: ", file_to_delete->name());
#endif
			delete file_to_delete;
			files.data.vec.remove(evt.index);
			tab_->view_files().cached_files_count = files.data.vec.size();
		}
		endRemoveRows();
		tab_->FilesChanged(FileCountChanged::Yes);
		break;
	}
	case io::FileEventType::Renamed: {
		if (evt.renaming_deleted_file_at != -1)
		{
			int real_index = evt.renaming_deleted_file_at;
			if (real_index > evt.index)
				real_index--;
			
			if (real_index >= 0) {
				beginRemoveRows(QModelIndex(), real_index, real_index);
				endRemoveRows();
			}
		}
		
		UpdateSingleRow(evt.index);
		tab_->FilesChanged(FileCountChanged::No, evt.index);
		break;
	}
	default: {
		mtl_trace();
	}
	} /// switch()
}

bool TableModel::InsertRows(const i32 at, const QVector<cornus::io::File*> &files_to_add)
{
	io::Files &files = tab_->view_files();
	{
		MutexGuard guard = files.guard();
		
		if (files.data.vec.isEmpty())
			return false;
	}
	
	const int first = at;
	const int last = at + files_to_add.size() - 1;
	
	beginInsertRows(QModelIndex(), first, last);
	{
		MutexGuard guard(&files.mutex);
		for (i32 i = 0; i < files_to_add.size(); i++)
		{
			auto *song = files_to_add[i];
			files.data.vec.insert(at + i, song);
		}
		tab_->view_files().cached_files_count = files.data.vec.size();
	}
	endInsertRows();
	
	return true;
}

bool TableModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (count <= 0)
		return false;
	
	const int first = row;
	const int last = row + count - 1;
	io::Files &files = tab_->view_files();
	
	beginRemoveRows(QModelIndex(), first, last);
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		for (int i = count - 1; i >= 0; i--) {
			const i32 index = first + i;
			auto *item = vec[index];
			vec.erase(vec.begin() + index);
			delete item;
		}
		tab_->view_files().cached_files_count = vec.size();
	}
	
	endRemoveRows();
	return true;
}

int TableModel::rowCount(const QModelIndex &parent) const
{
	return tab_->view_files().cached_files_count;
}

void TableModel::SelectFilenamesLater(const QVector<QString> &v, const SameDir sd)
{
	io::Files &files = tab_->view_files();
	MutexGuard guard = files.guard();
	auto dir_to_skip = (sd == SameDir::Yes) ? -1 : files.data.dir_id;
	files.data.skip_dir_id = dir_to_skip;
	for (const auto &s: v) {
		files.data.filenames_to_select.insert(s, 0);
	}
}

void TableModel::SwitchTo(io::FilesData *new_data)
{
	io::Files &files = tab_->view_files();
	int prev_count, new_count;
	{
		MutexGuard guard = files.guard();
		prev_count = files.data.vec.size();
		new_count = new_data->vec.size();
	}
	
	beginRemoveRows(QModelIndex(), 0, prev_count - 1);
	{
		MutexGuard guard = files.guard();
		for (auto *file: files.data.vec)
			delete file;
		files.data.vec.clear();
		tab_->table()->ClearMouseOver();
		files.cached_files_count = files.data.vec.size();
	}
	endRemoveRows();
	
	beginInsertRows(QModelIndex(), 0, new_count - 1);
	i32 dir_id;
	{
		MutexGuard guard = files.guard();
		dir_id = ++files.data.dir_id;
		files.data.processed_dir_path = new_data->processed_dir_path;
		files.data.can_write_to_dir(new_data->can_write_to_dir());
		/// copying sorting order is logically wrong because it overwrites
		/// the existing one.
		files.data.show_hidden_files(new_data->show_hidden_files());
		files.data.vec = new_data->vec;
		new_data->vec.clear();
		files.cached_files_count = files.data.vec.size();
	}
	endInsertRows();
	
	QVector<int> indices;
	tab_->table()->SyncWith(app_->clipboard(), indices);
	WatchArgs *args = new WatchArgs {
		.dir_id = dir_id,
		.dir_path = new_data->processed_dir_path,
		.table_model = this,
	};
	
	UpdateIndices(indices);
	UpdateHeaderNameColumn();
	InotifyBatchFinished();
	tab_->DisplayingNewDirectory(dir_id);
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::gui::WatchDir, args);
	if (status != 0) {
		mtl_status(status);
	}
}

void TableModel::UpdateIndices(const QVector<int> &indices)
{
	if (indices.isEmpty())
		return;
	
	int min = -1, max = -1;
	bool initialize = true;
	
	for (int next: indices) {
		if (initialize) {
			initialize = false;
			min = next;
			max = next;
		} else {
			if (next < min)
				min = next;
			else if (next > max)
				max = next;
		}
	}
	
	const int count_per_page = tab_->table()->GetVisibleRowsCount();
	
	if (min == -1 || max == -1) {
		UpdateVisibleArea();
	} else if ((max - min) > count_per_page) {
//		mtl_trace("Max: %d, min: %d, count per page: %d",
//			max, min, count_per_page);
		UpdateVisibleArea();
	} else {
		UpdateRowRange(min, max);
	}
}

void TableModel::UpdateRange(int row1, Column c1, int row2, Column c2)
{
	int first, last;
	
	if (row1 > row2) {
		first = row2;
		last = row1;
	} else {
		first = row1;
		last = row2;
	}
	
	const QModelIndex top_left = createIndex(first, int(c1));
	const QModelIndex bottom_right = createIndex(last, int(c2));
	Q_EMIT dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

void TableModel::UpdateVisibleArea()
{
	gui::Table *table = tab_->table();
	int row_start = table->verticalScrollBar()->value() / table->GetRowHeight();
	int count_per_page = table->GetVisibleRowsCount();
	UpdateRowRange(row_start, row_start + count_per_page);
}

void TableModel::UpdateHeaderNameColumn()
{
	if (app_->prefs().show_free_partition_space()) {
		cached_free_space_ = app_->GetPartitionFreeSpace();
	}
	const int col = (int)Column::FileName;
	headerDataChanged(Qt::Horizontal, col, col);
}

}
