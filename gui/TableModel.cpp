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
#include "../uring.hh"

#include <sys/epoll.h>
#include <cstring>
#include <sys/ioctl.h>
#include <liburing.h>
#include <QFont>
#include <QScrollBar>
#include <QTime>
#include <QTimer>

//#define CORNUS_DEBUG_INOTIFY_BATCH
#define CORNUS_DEBUG_INOTIFY

namespace cornus::gui {

const auto ConnectionType = Qt::BlockingQueuedConnection;
//const size_t kInotifyEventBufLen = 16 * (sizeof(struct inotify_event) + NAME_MAX + 1);

struct RenameData {
// helper struct to deal with inotify's shitty rename support.
	QString name;
	u4 cookie = 0;
	i1 checked_times = 0;
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

QString TakeTheOtherNameByCookie(Renames &ren, const u4 cookie)
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
	const QString new_name, const i4 dir_id)
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
	const QString name, const i4 dir_id)
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
	const QString name, const i4 dir_id, const io::CloseWriteEvent cwe)
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
	const int dir_id;
	const int wd;
	Renames &renames;
	const QProcessEnvironment &env;
};

struct EventChain {
	std::list<EventArgs*> list;
	CondMutex cm = {};
	bool must_exit = false;
};

void ProcessEvents(EventArgs *a);
void* InotifyTh(void *args)
{
mtl_info("============InotifyTh");
	pthread_detach(pthread_self());
	EventChain *ec = (EventChain*)args;
	while (true)
	{
		EventArgs *a = nullptr;
		{
			auto g = ec->cm.guard();
			while (ec->list.empty() && !ec->must_exit)
			{
				ec->cm.CondWait();
			}
			
			if (ec->must_exit)
				break;
			
			a = ec->list.front();
			ec->list.pop_front();
		}
		if (a)
			ProcessEvents(a);
	}
	
	delete ec;
	return nullptr;
}

void ProcessEvents(EventArgs *a)
{
mtl_info("<EVT BATCH>");
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
			mtl_info("%s", ev->name);
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
mtl_info("Before lock()");
			ren.m.Lock();
mtl_info("After lock()");
			ren.vec.append(RenameData {
				.name = name,
				.cookie = ev->cookie,
				.checked_times = 0 });
			ren.m.Unlock();
		} else if (mask & IN_MOVED_TO) {
			const auto &new_name = name;
			Renames &ren = a->renames;
			mtl_info("IN_MOVED_TO before lock()");
			ren.m.Lock();
			mtl_info("IN_MOVED_TO after lock()");
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
mtl_trace("new_file != nullptr");
					selected = new_file->is_selected();
					new_icon = new_file->cache().icon;
					files_vec.remove(new_file_index);
					delete_file_index = new_file_index;
					delete new_file;
				} else {
mtl_trace("new_file == nullptr");
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
					old_file->cache().icon = new_icon;
				}
				
				if (old_file->size() == -1)
				{
mtl_info("Reloading meta");
					PrintErrors pe = PrintErrors::No;
#ifdef CORNUS_DEBUG_INOTIFY
					pe = PrintErrors::Yes;
#endif
					cbool ok = io::ReloadMeta(*old_file, stx, a->env, pe);
					Q_UNUSED(ok);
#ifdef CORNUS_DEBUG_INOTIFY
					mtl_info("Reloaded meta: %s: %d", qPrintable(old_file->name()), ok);
#endif
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
	
	mtl_info("</EVT BATCH>");
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
	const int notify_fd = notify.fd;
	QProcessEnvironment env = app->env();
	
	io::Files &files = tab->view_files();
	files.Lock();
	const int signal_quit_fd = files.data.signal_quit_fd;
	files.Unlock();
	mtl_info(" === WatchDir() inotify fd: %d, signal_fd: %d", notify_fd, signal_quit_fd);
	const auto path = args->dir_path.toLocal8Bit();
	const auto event_types = IN_ATTRIB | IN_CREATE | IN_DELETE
		| IN_DELETE_SELF | IN_MOVE_SELF | IN_CLOSE_WRITE
		| IN_MOVE;// | IN_MODIFY;
	const int wd = inotify_add_watch(notify_fd, path.data(), event_types);
	if (wd == -1)
	{
		mtl_status(errno);
		return nullptr;
	}
	
	io::AutoRemoveWatch arw(notify, wd);
	/* const int kQueueDepth = 2; // signal_fd and inotify_fd
	struct io_uring ring;
	io_uring_queue_init(kQueueDepth, &ring, 0);
	QVector<uring::BufArg> buf_args = {
		{signal_quit_fd, 8},
		{notify_fd, 4096 * 10},
	};
	
	MTL_CHECK_ARG(uring::SubmitBuffers(&ring, buf_args), nullptr); */
	
	files.Lock();
	files.data.thread_exited(false);
	files.Unlock();
	
	Renames renames = {};
	mtl_info("Thread: %lX, notify_fd: %d, signal_quit_fd: %d", i8(pthread_self()), notify_fd, signal_quit_fd);
	CondMutex has_been_unmounted_or_deleted = {};
//	using TimeSpec = struct __kernel_timespec;
//	TimeSpec ts = {};
	//uring::UserData *data;
	
	EventChain *ec = new EventChain();
	io::NewThread(InotifyTh, ec);
	
	const int epoll_fd = epoll_create(1);
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

		mtl_info("Listening for %d", evt.data.fd);
	}
	
	while (true)
	{
		//TimeSpec *ts_param;
		//data = nullptr;
		bool empty;
		{
			auto g = renames.m.guard();
			empty = renames.vec.isEmpty();
		}
		
		cint ms = empty ? 50000 : 20;
		cint num_fds = epoll_wait(epoll_fd, evt_vec.data(), evt_vec.size(), ms);
		if (num_fds == -1)
		{
			mtl_status(errno);
			break;
		}
		
		bool signalled_from_event_fd = false;
		for (int i = 0; i < num_fds; i++)
		{
			const auto &evt = evt_vec[i];
			const bool readable = evt.events & EPOLLIN;
			if (!readable)
				continue;

			mtl_info("fd: %d", evt.data.fd);
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
	mtl_info("Inotify fd, after lock()");
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
				{
					auto g = ec->cm.guard();
					ec->list.push_back(a);
					ec->cm.Broadcast();
				}
			} else if (evt.data.fd == signal_quit_fd) {
				//mtl_info("Quit fd!");
				signalled_from_event_fd = true;
				// must read 8 bytes:
				i8 num;
				read(evt.data.fd, &num, sizeof num);
			} else {
				mtl_trace();
			}
		}
		
		
		if (signalled_from_event_fd)
			break;
		
		/*
		struct io_uring_cqe *cqe = nullptr;
//If ts is specified, the application need not call io_uring_submit()
// before calling this function, as it will be done internally.
// From this it also follows that this function isn’t safe to use for
// applications that split SQ and CQ handling between two threads and
// expect that to work without synchronization, as this function
// manipulates both the SQ and CQ side.
		cint ret = io_uring_wait_cqe_timeout(&ring, &cqe, ts_param);
		bool time_expired = false;
		if (ret < 0)
		{
			errno = -ret;
			if (errno == ETIME)
			{
				time_expired = true;
				mtl_info("Time Expired...");
			} else {
				mtl_errno();
				break;
			}
		}
mtl_info("time_expired: %d", time_expired);
		if (!time_expired)
		{
			data = (uring::UserData*) io_uring_cqe_get_data(cqe);
			cint num_read = cqe->res;
			if (num_read < 0)
			{
				mtl_warn("Async readv failed: %s, fd: %d", strerror(-num_read), data->fd);
				break;
			}
			
			if (data->fd == signal_quit_fd)
			{
	mtl_info("Quit signal fd");
				break;
			} else if (data->fd == notify_fd) {
	mtl_info("Inotify fd, before lock()");
				files.Lock();
	mtl_info("Inotify fd, after lock()");
				const bool include_hidden_files = files.data.show_hidden_files();
				files.Unlock();
				EventArgs *a = new EventArgs {
					.buf = new char[num_read],
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
				std::memcpy(a->buf, data->iv.iov_base, num_read);
				io_uring_cqe_seen(&ring, cqe);
				{
					auto g = ec->cm.guard();
					ec->list.push_back(a);
					ec->cm.Broadcast();
				}
			} else {
				mtl_trace("No fd matched");
				continue;
			}
		} */
		
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
		
mtl_info("before renames.m.Lock()");
		renames.m.Lock();
mtl_info("after renames.m.Lock()");
		empty = renames.vec.isEmpty();
		renames.m.Unlock();
		if (!empty)
		{
mtl_info("rename_vec not empty");
			ReinterpretRenames(renames, args->table_model, &files, args->dir_id);
		} else {
mtl_info("rename_vec empty");
		}
		
		renames.m.Lock();
		empty = renames.vec.isEmpty();
		renames.m.Unlock();
		
		if (empty)
		{
			files.Lock();
			const bool call_event_func = !files.data.filenames_to_select.isEmpty()
				&& !files.data.should_skip_selecting();
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
	}
	
	//io_uring_queue_exit(&ring);
	{
		auto g = ec->cm.guard();
		ec->must_exit = true;
		ec->cm.Signal();
	}
	
	{
		auto g = files.guard();
		files.data.thread_exited(true);
		files.Broadcast();
	}
	
	mtl_info("Thread %lX exited", i8(pthread_self()));
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
		auto &select_list = files.data.filenames_to_select;
		if (select_list.isEmpty() || files.data.should_skip_selecting())
			return;
		
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
				if (file->name() == name)
				{
mtl_printq2("Selecting name ", name);
					indices.insert(i);
					file->set_selected(true);
					it = select_list.erase(it);
					found = true;
					break;
				}
			}
			
			if (!found)
			{
				const int new_count = it.value() + 1;
				if (new_count > 5) {
mtl_printq2("Removed from list: ", name);
					it = select_list.erase(it);
				} else {
mtl_printq2("Increased counter for ", name);
					select_list[name] = new_count;
					it++;
				}
			}
		}
	}
	
	if (!indices.isEmpty())
	{
		tab_->UpdateIndices(indices);
		const int first_file_index = *indices.constBegin();
		//mtl_info("scroll to: %d", index);
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
			const int count = files_vec.size();
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

bool TableModel::InsertRows(const i4 at, const QVector<cornus::io::File*> &files_to_add)
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
		for (i4 i = 0; i < files_to_add.size(); i++)
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
			const i4 index = first + i;
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
//			while (!files.data.thread_exited()) {
//				files.CondWait();
//			}
//			files.data.thread_exited(false);
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
	i4 dir_id;
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
		const int next = it.next();
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
	const int col = (int)Column::FileName;
	headerDataChanged(Qt::Horizontal, col, col);
}

}
