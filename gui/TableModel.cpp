#include "TableModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "../io/Notify.hpp"
#include "Location.hpp"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "Tab.hpp"
#include "Table.hpp"
#include "TableHeader.hpp"

#include <sys/epoll.h>
#include <cstring>
#include <sys/ioctl.h>
#include <QFont>
#include <QScrollBar>
#include <QTime>
#include <QTimer>

#define CORNUS_DEBUG_INOTIFY

namespace cornus::gui {

const auto ConnectionType = Qt::BlockingQueuedConnection;

struct RenameData {
// helper struct to deal with inotify's shitty rename support.
	QString name;
	u32 cookie = 0;
	i8 checked_times = 0;
};

struct Renames {
	QVector<RenameData> vec;
	Mutex m = {};
};

struct WatchArgs {
	DirId dir_id = 0;
	QString dir_path;
	cornus::gui::TableModel *table_model = nullptr;
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

QString TakeTheOtherNameByCookie(Renames &ren, const u32 cookie)
{
	cint count = ren.vec.size();
	for (int i = 0; i < count; i++)
	{
		cauto &next = ren.vec[i];
		if (next.cookie == cookie)
			return ren.vec.takeAt(i).name;
	}
	
	return QString();
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
	auto &env = model->app()->env();
	if (!io::ReloadMeta(*new_file, stx, env, PrintErrors::No))
	{
		delete new_file;
		return;
	}
	
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
		auto g = files->guard();
		Find(files->data.vec, name, &index);
	}
	
	MTL_CHECK_VOID(index != -1);
	io::FileEvent evt = {};
	evt.dir_id = dir_id;
	evt.index = index;
	evt.type = io::FileEventType::Deleted;
	QMetaObject::invokeMethod(model, "InotifyEvent",
		ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
}

void SendModifiedEvent(TableModel *model, cornus::io::Files *files,
	const QString name, const i32 dir_id, const io::CloseWriteEvent cwe)
{
	int index = -1;
	io::File *found = nullptr;
	{
		auto g = files->guard();
		found = Find(files->data.vec, name, &index);
		MTL_CHECK_VOID(found != nullptr);
		struct statx stx;
		PrintErrors pe = PrintErrors::No;
#ifdef CORNUS_DEBUG_INOTIFY
		pe = PrintErrors::Yes;
#endif
		if (!io::ReloadMeta(*found, stx, model->app()->env(), pe))
		{
			found->size(-1);
		}
		
		found = (cwe == io::CloseWriteEvent::Yes) ? found->Clone() : nullptr;
	}
	
	io::FileEvent evt = {};
	evt.new_file = found;
	evt.dir_id = dir_id;
	evt.index = index;
	evt.type = io::FileEventType::Modified;
	QMetaObject::invokeMethod(model, "InotifyEvent",
		ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
}

struct EventArgs {
	
	~EventArgs()
	{
		delete[] buf;
	}
	char *buf;
	const isize num_read;
	cornus::io::Files *files;
	CondMutex &has_been_unmounted_or_deleted;
	const bool include_hidden_files;
	cornus::gui::TableModel *model;
	cint dir_id;
	cint wd;
	Renames &renames;
	const QProcessEnvironment &env;
};

void ProcessEvents(EventArgs *a)
{
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("<EVT BATCH>");
#endif
	AutoDelete a_(a);
	struct statx stx;
	isize add = 0;
	auto *model = a->model;
	const char *end = a->buf + a->num_read;
	for (char *p = a->buf; p < end; p += add)
	{
		struct inotify_event *ev = (struct inotify_event*) p;
		add = sizeof(struct inotify_event) + ev->len;
		if (ev->wd != a->wd)
		{
			mtl_trace("ev->wd: %d, wd: %d", ev->wd, a->wd);
			continue;
		}
		
		QString name;
		if (ev->len > 0)
		{
			name = ev->name;
			if (!a->include_hidden_files && name.startsWith('.'))
				continue;
#ifdef CORNUS_DEBUG_INOTIFY
			mtl_info("%s", ev->name);
#endif
		}
		
		const auto mask = ev->mask;
		//const bool is_dir = mask & IN_ISDIR;
		if (mask & (IN_ATTRIB | IN_MODIFY))
		{
#ifdef CORNUS_DEBUG_INOTIFY
mtl_trace("(IN_ATTRIB | IN_MODIFY): %s", ev->name);
#endif		
			SendModifiedEvent(model, a->files, name, a->dir_id, io::CloseWriteEvent::No);
		} else if (mask & IN_CREATE) {
#ifdef CORNUS_DEBUG_INOTIFY
mtl_trace("IN_CREATE: %s", ev->name);
#endif
			SendCreateEvent(model, a->files, name, a->dir_id);
		} else if (mask & IN_DELETE) {
#ifdef CORNUS_DEBUG_INOTIFY
mtl_trace("IN_DELETE: %s", ev->name);
#endif
			SendDeleteEvent(model, a->files, name, a->dir_id);
		} else if (mask & IN_DELETE_SELF) {
			mtl_warn("IN_DELETE_SELF");
			a->has_been_unmounted_or_deleted.SetFlag(true);
			break;
		} else if (mask & IN_MOVE_SELF) {
			mtl_warn("IN_MOVE_SELF");
		} else if (mask & IN_MOVED_FROM) {
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("IN_MOVED_FROM, from: %s, len: %d, cookie: %d",
	qPrintable(name), ev->len, ev->cookie);
#endif
			auto &ren = a->renames;
			ren.m.Lock();
			ren.vec.append(RenameData {
				.name = name,
				.cookie = ev->cookie,
				.checked_times = 0 });
			ren.m.Unlock();
		} else if (mask & IN_MOVED_TO) {
			const auto &new_name = name;
			Renames &ren = a->renames;
			ren.m.Lock();
			QString old_name = TakeTheOtherNameByCookie(ren, ev->cookie);
			ren.m.Unlock();
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("IN_MOVED_TO new_name: %s, from_name: %s, cookie %d",
	qPrintable(name), qPrintable(old_name), ev->cookie);
#endif
			if (old_name.isEmpty())
			{
				SendCreateEvent(model, a->files, new_name, a->dir_id);
				return;
			}
			int delete_file_index = -1, new_file_index = -1, old_file_index = -1;
			io::File *cloned_file = nullptr;
			{
				auto g = a->files->guard();
				auto &files_vec = a->files->data.vec;
				io::File *new_file = Find(files_vec, new_name, &new_file_index);
				QIcon *new_icon = nullptr;
				bool selected = false;
				if (new_file != nullptr)
				{
mtl_trace();
					selected = new_file->is_selected();
					new_icon = new_file->cache().icon;
					files_vec.remove(new_file_index);
					delete_file_index = new_file_index;
					delete new_file;
				}
				
				io::File *old_file = Find(files_vec, old_name, &old_file_index);
				if (old_file == nullptr)
				{
					mtl_warn("File %s not found", ev->name);
					continue;
				}
				old_file->name(new_name);
				old_file->set_selected(selected);
				if (new_icon != nullptr)
				{
mtl_trace();
					old_file->cache().icon = new_icon;
				}
				
				if (old_file->size() == -1)
				{
					PrintErrors pe = PrintErrors::No;
#ifdef CORNUS_DEBUG_INOTIFY
					pe = PrintErrors::Yes;
#endif
					io::ReloadMeta(*old_file, stx, a->env, pe);
				}
				cloned_file = old_file->Clone();
				std::sort(files_vec.begin(), files_vec.end(), cornus::io::SortFiles);
				Find(files_vec, new_name, &new_file_index);
			}
			io::FileEvent evt = {};
			evt.new_file = cloned_file;
			evt.dir_id = a->dir_id;
			evt.renaming_deleted_file_at = delete_file_index;
			evt.index = new_file_index;
			evt.type = io::FileEventType::Renamed;
			QMetaObject::invokeMethod(model, "InotifyEvent",
			ConnectionType, Q_ARG(cornus::io::FileEvent, evt));
		} else if (mask & IN_Q_OVERFLOW) {
			mtl_warn("IN_Q_OVERFLOW");
		} else if (mask & IN_UNMOUNT) {
			a->has_been_unmounted_or_deleted.SetFlag(true);
			break;
		} else if (mask & IN_CLOSE_WRITE) {
			QString name(ev->name);
			if (!a->include_hidden_files && name.startsWith('.'))
				continue;
			
#ifdef CORNUS_DEBUG_INOTIFY
			if (ev->len > 0)
				mtl_trace("IN_CLOSE: %s", ev->name);
			else
				mtl_trace("IN_CLOSE");
#endif
			SendModifiedEvent(model, a->files, name, a->dir_id, io::CloseWriteEvent::Yes);
		} else if (mask & (IN_IGNORED | IN_CLOSE_NOWRITE)) {
		} else {
			mtl_warn("Unhandled inotify event: %u", mask);
		}
	}
	
	#ifdef CORNUS_DEBUG_INOTIFY
	mtl_info("</EVT BATCH>");
	#endif
}

void ReinterpretRenames(Renames &ren,
	TableModel *model, cornus::io::Files *files, const DirId dir_id)
{
	auto g = ren.m.guard();
	for (int i = ren.vec.size() - 1; i >= 0; i--)
	{
		RenameData &next = ren.vec[i];
		next.checked_times++;
		if (next.checked_times > 2)
		{
			// It means that the proper IN_MOVED_TO never arrived, which
			// means the file was moved to another folder,
			// not renamed in place, so it's the same as a delete event:
			SendDeleteEvent(model, files, next.name, dir_id);
			ren.vec.remove(i);
#ifdef CORNUS_DEBUG_INOTIFY
			mtl_info("Removed from rename_vec: %s", qPrintable(next.name));
#endif
		}
	}
}

void* WatchDir(void *void_args)
{
	pthread_detach(pthread_self());
	MTL_CHECK_ARG(void_args != nullptr, nullptr);
	
	WatchArgs *args = (WatchArgs*)void_args;
	TableModel *model = args->table_model;
	Tab *tab = model->tab();
	App *app = tab->app();
	AutoDelete args_(args);
	io::Notify &notify = tab->notify();
	cint notify_fd = notify.fd;
	QProcessEnvironment env = app->env();
	
	io::Files &files = tab->view_files();
	files.Lock();
	cint signal_quit_fd = files.data.signal_quit_fd;
	files.Unlock();
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_info(" === WatchDir() inotify fd: %d, signal_fd: %d", notify_fd, signal_quit_fd);
#endif
	const auto path = args->dir_path.toLocal8Bit();
	const auto event_types = IN_ATTRIB | IN_CREATE | IN_DELETE
		| IN_DELETE_SELF | IN_MOVE_SELF | IN_CLOSE_WRITE
		| IN_MOVE;// | IN_MODIFY;
	cint wd = inotify_add_watch(notify_fd, path.data(), event_types);
	if (wd == -1)
	{
		mtl_status(errno);
		return nullptr;
	}
	
	io::AutoRemoveWatch arw(notify, wd);

	files.Lock();
	files.data.thread_exited(false);
	files.Unlock();
	
	Renames renames = {};
	CondMutex has_been_unmounted_or_deleted = {};
	cint epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		mtl_status(errno);
		return 0;
	}

	AutoCloseFd epoll_fd_(epoll_fd);
	QVector<struct epoll_event> evt_vec(2);
	evt_vec[0].events = EPOLLIN;
	evt_vec[0].data.fd = notify_fd;
	evt_vec[1].events = EPOLLIN;
	evt_vec[1].data.fd = signal_quit_fd;

	for (struct epoll_event &evt: evt_vec)
	{
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, evt.data.fd, &evt))
		{
			mtl_status(errno);
			return nullptr;
		}
	}
	
	while (true)
	{
		bool ren_is_empty;
		{
			auto g = renames.m.guard();
			ren_is_empty = renames.vec.isEmpty();
		}
		
		cint ms = ren_is_empty ? 50000 : 20;
#ifdef CORNUS_DEBUG_INOTIFY
mtl_info("ms: %d", ms);
#endif
		cint num_fds = epoll_wait(epoll_fd, evt_vec.data(), evt_vec.size(), ms);
		if (num_fds == -1)
		{
			if (errno == EINTR)
			{ // Interrupted system call after resume from sleep
				continue;
			}
			mtl_status(errno);
			break;
		}
		
//		static int gg = 0;
//		mtl_info("%d", gg++);
		
		bool signalled_from_event_fd = false;
		for (int i = 0; i < num_fds; i++)
		{
			const auto &evt = evt_vec[i];
			if ((evt.events & EPOLLIN) == 0)
				continue;
#ifdef CORNUS_DEBUG_INOTIFY
			mtl_info("fd: %d", evt.data.fd);
#endif
			if (evt.data.fd == notify_fd)
			{
				cisize buf_len = 4096 * 4;
				char *buf = new char[buf_len];
				
				cisize num_read = read(evt.data.fd, buf, buf_len);
				if (num_read <= 0) {
					if (num_read == -1)
						mtl_status(errno);
					return nullptr;
				}
				files.Lock();
				const bool include_hidden_files = files.data.show_hidden_files();
				files.Unlock();
				EventArgs *a = new EventArgs {
					.buf = buf,
					.num_read = num_read,
					.files = &files,
					.has_been_unmounted_or_deleted = has_been_unmounted_or_deleted,
					.include_hidden_files = include_hidden_files,
					.model = model,
					.dir_id = args->dir_id,
					.wd = wd,
					.renames = renames,
					.env = env
				};
				//std::memcpy(buf, data->iv.iov_base, num_read);
				ProcessEvents(a);
			} else if (evt.data.fd == signal_quit_fd) {
				//mtl_info("Quit fd!");
				signalled_from_event_fd = true;
				// must read 8 bytes:
				io::ReadEventFd(evt.data.fd);
			} else {
				mtl_trace();
			}
		}
		
		
		if (signalled_from_event_fd)
			break;
		
		if (has_been_unmounted_or_deleted.GetFlag())
		{
mtl_info("has_been_unmounted_or_deleted");
			arw.RemoveWatch(wd);
			break;
		}
		
		{
			auto g = files.guard();
			if (files.data.thread_must_exit() || (args->dir_id != files.data.dir_id))
			{
				mtl_info("thread must exit");
				break;
			}
		}
		
		renames.m.Lock();
		cint count_was = renames.vec.size();
		renames.m.Unlock();
		if (count_was > 0)
			ReinterpretRenames(renames, args->table_model, &files, args->dir_id);
		
#ifdef CORNUS_DEBUG_INOTIFY
		renames.m.Lock();
		cint count_is = renames.vec.size();
		renames.m.Unlock();
		mtl_trace("was: %d, is: %d", count_was, count_is);
#endif

		files.Lock();
		cint fn_count = files.data.filenames_to_select.size();
		cbool call_event_func = fn_count > 0 && !files.data.should_skip_selecting();
		files.Unlock();
		if (call_event_func)
		{
			/* This must be dispatched as "QueuedConnection" (low priority)
			to allow the previous
			inotify events to be processed first, otherwise file selection doesn't
			get preserved because inotify events arrive at random pace. */
			QMetaObject::invokeMethod(model, "SelectFilesAfterInotifyBatch", Qt::QueuedConnection);
		}
	}
	
	{
		auto g = files.guard();
		files.data.thread_exited(true);
		files.Broadcast();
	}
	
#ifdef CORNUS_DEBUG_INOTIFY
	mtl_info("Thread %lX exited", i64(pthread_self()));
#endif
	return nullptr;
}

TableModel::TableModel(cornus::App *app, gui::Tab *tab): app_(app), tab_(tab)
{}

TableModel::~TableModel() {}

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

void TableModel::SelectFilesAfterInotifyBatch()
{
	QSet<int> indices;
	auto &files = tab_->view_files();
	{
		auto g = files.guard();
		auto &names_to_select = files.data.filenames_to_select;
		if (names_to_select.isEmpty() || files.data.should_skip_selecting())
			return;
		
		QVector<io::File*> &files_vec = files.data.vec;
		auto it = names_to_select.begin();
		cint files_count = files_vec.size();
		
		while (it != names_to_select.end())
		{
			const QString &select_name = it.key();
			bool found = false;
			for (int i = 0; i < files_count; i++)
			{
				io::File *file = files_vec[i];
				if (file->name() == select_name)
				{
#ifdef CORNUS_DEBUG_INOTIFY
mtl_printq2("Selecting name ", select_name);
#endif
					indices.insert(i);
					file->set_selected(true);
					it = names_to_select.erase(it);
					found = true;
					break;
				}
			}
			
			if (!found)
			{
				cint new_count = it.value() + 1;
				if (new_count > 5) {
mtl_printq2("Removed from list: ", select_name);
					it = names_to_select.erase(it);
				} else {
mtl_printq2("Increased counter for ", select_name);
					names_to_select[select_name] = new_count;
					it++;
				}
			}
		}
	}
	
	if (!indices.isEmpty())
	{
		tab_->UpdateIndices(indices);
		cint first_file_index = *indices.constBegin();
		//mtl_info("scroll to: %d", first_file_index);
		tab_->ScrollToFile(first_file_index);
	}
}

void TableModel::InotifyEvent(cornus::io::FileEvent evt)
{
	auto &files = tab_->view_files();
	{
		auto g = files.guard();
		if (evt.dir_id != files.data.dir_id)
		{
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
		tab_->FileChanged(evt.type, evt.new_file);
		break;
	}
	case io::FileEventType::Created: {
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_printq2("CREATED: ", evt.new_file->name());
#endif
		io::File *cloned_file = nullptr;
		int index = -1;
		{
			files.Lock();
			QVector<io::File*> &files_vec = files.data.vec;
			cint count = files_vec.size();
			for (int i = 0; i < count; i++)
			{
				io::File *next = files_vec[i];
				if (next->name() == evt.new_file->name())
				{
					evt.new_file->CopyBits(next);
					cloned_file = evt.new_file->Clone();
					delete next;
					files_vec[i] = evt.new_file;
					evt.new_file = nullptr;
					files.Unlock();
					tab_->FileChanged(io::FileEventType::Modified, cloned_file);
					return;
				}
			}
			
			index = FindPlace(evt.new_file, files_vec);
			files.Unlock();
		}
		beginInsertRows(QModelIndex(), index, index);
		{
			auto g = files.guard();
			evt.new_file->CountDirFiles();
			files.data.vec.insert(index, evt.new_file);
			cloned_file = evt.new_file->Clone();
			evt.new_file = nullptr;
			files.cached_files_count = files.data.vec.size();
		}
		endInsertRows();
		tab_->FileChanged(evt.type, cloned_file);
		break;
	}
	case io::FileEventType::Deleted: {
		beginRemoveRows(QModelIndex(), evt.index, evt.index);
		{
			auto g = files.guard();
			io::File *file_to_delete = files.data.vec[evt.index];
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_printq2("DELETED: ", file_to_delete->name());
#endif
			delete file_to_delete;
			files.data.vec.remove(evt.index);
			files.cached_files_count = files.data.vec.size();
		}
		endRemoveRows();
		tab_->FileChanged(evt.type);
		break;
	}
	case io::FileEventType::Renamed: {
#ifdef CORNUS_DEBUG_INOTIFY
		mtl_trace("RENAMED, index: %d", evt.index);
#endif
		if (evt.renaming_deleted_file_at != -1)
		{
			int real_index = evt.renaming_deleted_file_at;
			if (real_index > evt.index)
				real_index--;
			if (real_index >= 0) {
mtl_info("real_index: %d, renaming_deleted_file_at: %d, evt.index: %d", real_index,
	evt.renaming_deleted_file_at, evt.index);
				beginRemoveRows(QModelIndex(), real_index, real_index);
				endRemoveRows();
				tab_->FileChanged(io::FileEventType::Modified, evt.new_file);
			}
		}
		
		UpdateSingleRow(evt.index);
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
	
	cint first = at;
	cint last = at + files_to_add.size() - 1;
	
	beginInsertRows(QModelIndex(), first, last);
	{
		auto g = files.guard();
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
	
	cint first = row;
	cint last = row + count - 1;
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

void TableModel::SwitchTo(io::FilesData *new_data)
{
	const Reload reload = new_data->reloaded() ? Reload::Yes : Reload::No;
	io::Files &files = tab_->view_files();
	int prev_count, new_count;
	{
		auto g = files.guard();
		prev_count = files.data.vec.size();
		new_count = new_data->vec.size();
		if (files.first_time) {
			files.first_time = false;
		} else {
			files.WakeUpInotify(Lock::No);
		}
	}
	
	beginRemoveRows(QModelIndex(), 0, prev_count - 1);
	{
		auto g = files.guard();
		auto &old_vec = files.data.vec;
		for (auto *file: old_vec)
			delete file;
		old_vec.clear();
		tab_->table()->ClearMouseOver();
		files.cached_files_count = 0;
	}
	endRemoveRows();
	
	beginInsertRows(QModelIndex(), 0, new_count - 1);
	i32 dir_id;
	{
		auto g = files.guard();
		dir_id = ++files.data.dir_id;
		files.data.processed_dir_path = new_data->processed_dir_path;
		files.data.can_write_to_dir(new_data->can_write_to_dir());
		/// copying sorting order is logically wrong because it overwrites
		/// the existing one.
		files.data.show_hidden_files(new_data->show_hidden_files());
		files.data.count_dir_files_1_level(new_data->count_dir_files_1_level());
		files.data.vec = new_data->vec;
		new_data->vec.clear();
		files.cached_files_count = files.data.vec.size();
	}
	endInsertRows();
	
	QSet<int> indices;
	tab_->table()->SyncWith(app_->clipboard(), indices);
	WatchArgs *args = new WatchArgs {
		.dir_id = dir_id,
		.dir_path = new_data->processed_dir_path,
		.table_model = this,
	};
	
	UpdateIndices(indices);
	UpdateHeaderNameColumn();
	SelectFilesAfterInotifyBatch();
	tab_->DisplayingNewDirectory(dir_id, reload);
	io::NewThread(gui::WatchDir, args);
}

void TableModel::UpdateIndices(const QSet<int> &indices)
{
	if (indices.isEmpty())
		return;
	
	int min = -1, max = -1;
	bool initialize = true;
	
	QSetIterator<int> it(indices);
	while (it.hasNext())
	{
		cint next = it.next();
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
	
	cint count_per_page = tab_->table()->GetVisibleRowsCount();
	
	if (min == -1 || max == -1) {
		UpdateVisibleArea();
	} else if ((max - min) > count_per_page) {
		UpdateVisibleArea();
	} else {
		UpdateFileIndexRange(min, max);
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
	UpdateFileIndexRange(row_start, row_start + count_per_page);
}

void TableModel::UpdateHeaderNameColumn()
{
	if (app_->prefs().show_free_partition_space()) {
		cached_free_space_ = app_->GetPartitionFreeSpace();
	}
	cint col = (int)Column::FileName;
	headerDataChanged(Qt::Horizontal, col, col);
}

}
