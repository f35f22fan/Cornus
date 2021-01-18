#include "SidePaneModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "SidePane.hpp"
#include "SidePaneItem.hpp"

#include <sys/epoll.h>
#include <QFont>
#include <QScrollBar>
#include <QTime>

//#define DEBUG_INOTIFY

namespace cornus::gui {
namespace sidepane {
void* LoadItems(void *args)
{
	pthread_detach(pthread_self());
	cornus::App *app = (cornus::App*) args;
	ByteArray buf;
	if (io::ReadFile(QLatin1String("/proc/mounts"), buf) != io::Err::Ok)
		return nullptr;
	
 mtl_info("Have read: %ld bytes", buf.size());
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	auto list = s.splitRef('\n');
	const QString prefix = QLatin1String("/dev/sd");
	const QString skip_mount = QLatin1String("/boot/");
	InsertArgs method_args;
	
	for (auto &line: list)
	{
		if (!line.startsWith(prefix))
			continue;
		
		auto args = line.split(" ");
		QStringRef mount_path = args[1];
		
		if (mount_path.startsWith(skip_mount))
			continue;
		
		auto *p = new gui::SidePaneItem();
		p->dev_path(args[0].toString());
		p->mount_path(mount_path.toString());
		p->fs(args[2].toString());
		p->type(gui::SidePaneItemType::Partition);
		p->Init();
		method_args.vec.append(p);
	}
	
	SidePaneItems &items = app->side_pane_items();
	items.Lock();
	while (!items.widgets_created) {
		int status = pthread_cond_wait(&items.cond, &items.mutex);
		if (status != 0) {
			mtl_warn("pthread_cond_wait: %s", strerror(status));
			break;
		}
	}
	//mtl_info("widgets created: %s", items.widgets_created ? "true": "false");
	items.Unlock();
	
	QMetaObject::invokeMethod(app->side_pane_model(),
		"InsertFromAnotherThread",
		Q_ARG(cornus::gui::InsertArgs, method_args));
	
	return nullptr;
}
}

struct WatchArgs {
	i32 dir_id = 0;
	QString dir_path;
	cornus::gui::SidePaneModel *table_model = nullptr;
};

const size_t kInotifyEventSize = sizeof (struct inotify_event);
const size_t kInotifyEventBufLen = 256 * (kInotifyEventSize + 16);

/**
io::File* Find(const QVector<io::File*> &vec,
	const QString &name, const bool is_dir, int *index)
{
	int i = -1;
	for (io::File *file : vec)
	{
		i++;
		if (is_dir != file->is_dir())
			continue;
		
		if (file->name() == name) {
			if (index != nullptr)
				*index = i;
			return file;
		}
	}
	
	return nullptr;
}

void InsertFile(io::File *new_file, QVector<io::File*> &files_vec,
	int *inserted_at)
{
	for (int i = files_vec.size() - 1; i >= 0; i--) {
		io::File *next = files_vec[i];
		if (!io::SortFiles(new_file, next)) {
			i++;
			files_vec.insert(i, new_file);
			if (inserted_at != nullptr)
				*inserted_at = i;
			return;
		}
	}
	
	if (inserted_at != nullptr)
		*inserted_at = 0;
	
	files_vec.insert(0, new_file);
}

void ReadEvent(int inotify_fd, char *buf, cornus::io::Files *files,
	QVector<int> &update_indices)
{
	const ssize_t num_read = read(inotify_fd, buf, kInotifyEventBufLen);
	if (num_read == 0) {
		mtl_trace();
		return;
	}
	
	if (num_read == -1) {
		mtl_status(errno);
		return;
	}

	ssize_t add = 0;
	QVector<io::File*> &files_vec = files->vec;
	
	for (char *p = buf; p < buf + num_read; p += add) {
		struct inotify_event *ev = (struct inotify_event*) p;
		add = sizeof(struct inotify_event) + ev->len;
		const auto mask = ev->mask;
		const bool has_name = ev->len > 0;
		const bool is_dir = mask & IN_ISDIR;
		
		if (mask & IN_ATTRIB) {
			if (has_name) {
				int update_index;
				io::File *found = Find(files_vec, ev->name, is_dir, &update_index);
				if (found != nullptr) {
					io::ReloadMeta(*found);
					update_indices.append(update_index);
				}
			}
		} else if (mask & IN_CREATE) {
#ifdef DEBUG_INOTIFY
mtl_trace("IN_CREATE: %s", ev->name);
#endif
			QString name(ev->name);
			if (!files->show_hidden_files && name.startsWith('.'))
				continue;
			
			io::File *new_file = new io::File(files);
			new_file->name(name);
			if (io::ReloadMeta(*new_file)) {
				int inserted_at;
				InsertFile(new_file, files_vec, &inserted_at);
				update_indices.append(inserted_at);
			} else {
				delete new_file;
				mtl_warn();
			}
		} else if (mask & IN_DELETE) {
#ifdef DEBUG_INOTIFY
mtl_trace("IN_DELETE: %s", ev->name);
#endif
			int index;
			auto *found = Find(files_vec, ev->name, is_dir, &index);
			if (found != nullptr) {
				update_indices.append(-1);
				files_vec.remove(index);
			} else {
#ifdef DEBUG_INOTIFY
				mtl_trace();
#endif
			}
		} else if (mask & IN_DELETE_SELF) {
			mtl_warn("IN_DELETE_SELF");
		} else if (mask & IN_MOVE_SELF) {
			mtl_warn("IN_MOVE_SELF");
		} else if (mask & IN_MOVED_FROM) {
#ifdef DEBUG_INOTIFY
mtl_trace("IN_MOVED_FROM: %s, is_dir: %d", ev->name, is_dir);
#endif
			int from_index;
			io::File *from_file = Find(files_vec, ev->name, is_dir, &from_index);
			if (from_file != nullptr) {
				update_indices.append(from_index);
				files_vec.remove(from_index);
			} else {
				mtl_trace();
			}
		} else if (mask & IN_MOVED_TO) {
#ifdef DEBUG_INOTIFY
mtl_trace("IN_MOVED_TO: %s, is_dir: %d", ev->name, is_dir);
#endif
			int to_index;
			io::File *to_file = Find(files_vec, ev->name, is_dir, &to_index);
			if (to_file != nullptr) {
				update_indices.append(to_index);
				to_file->ClearCache();
				files_vec.remove(to_index);
			} else {
				to_file = new io::File(files);
			}
			to_file->name(ev->name);
			io::ReloadMeta(*to_file);
			/// Reinsert at the proper position:
			InsertFile(to_file, files_vec, &to_index);
			update_indices.append(to_index);
		} else if (mask & IN_Q_OVERFLOW) {
			mtl_warn("IN_Q_OVERFLOW");
		} else if (mask & IN_UNMOUNT) {
			mtl_warn("IN_UNMOUNT");
		} else if (mask & IN_CLOSE) {
//#ifdef DEBUG_INOTIFY
//mtl_trace("IN_CLOSE: %s", ev->name);
//#endif
			int update_index;
			io::File *found = Find(files_vec, ev->name, is_dir, &update_index);
			
			if (found == nullptr)
				continue;
			
			if (io::ReloadMeta(*found)) {
				update_indices.append(update_index);
			} else {
				mtl_trace();
			}
		} else if (mask & IN_IGNORED) {
		} else {
			mtl_warn("Unhandled inotify event: %u", mask);
		}
	}
}

void* WatchDir(void *void_args)
{
	pthread_detach(pthread_self());
	CHECK_PTR_NULL(void_args);
	
	WatchArgs *args = (WatchArgs*)void_args;
	AutoDelete ad_args(args);
	//mtl_info("kInotifyEventBufLen: %ld", kInotifyEventBufLen);
	char *buf = new char[kInotifyEventBufLen];
	AutoDeleteArr ad_arr(buf);
	cornus::gui::Notify &notify = args->table_model->notify();
	
	auto path = args->dir_path.toLocal8Bit();
	auto event_types = IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF
		| IN_MOVE_SELF | IN_CLOSE | IN_MOVE;// | IN_MODIFY;
	int wd = inotify_add_watch(notify.fd, path.data(), event_types);
	if (wd == -1) {
		mtl_status(errno);
		return nullptr;
	}
	AutoRemoveWatch arw(notify, wd);
	//mtl_info("Watching %s using wd %d", path.data(), wd);
	
	int epfd = epoll_create(1);
	
	if (epfd == -1) {
		mtl_status(errno);
		return 0;
	}
	
	struct epoll_event pev;
	pev.events = EPOLLIN;
	pev.data.fd = notify.fd;
	
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify.fd, &pev)) {
		mtl_status(errno);
		close(epfd);
		return nullptr;
	}
	
	struct epoll_event poll_event;
	const int seconds = 3 * 1000;
	cornus::io::Files *files = args->table_model->files();
	UpdateTableArgs method_args;
	method_args.dir_id = args->dir_id;
	
	while (true)
	{
		{
			MutexGuard guard(&files->mutex);
			
			if (args->dir_id != files->dir_id)
				break;
		}
		
		int event_count = epoll_wait(epfd, &poll_event, 1, seconds);
		{
			MutexGuard guard(&files->mutex);
			
			if (args->dir_id != files->dir_id)
				break;
			
			method_args.prev_count = files->vec.size();
			
			if (event_count > 0)
				ReadEvent(poll_event.data.fd, buf, files,
					method_args.indices);
			
			method_args.new_count = files->vec.size();
		}
		
//		if (!renames.isEmpty()) {
//			mtl_info("Not all renames happened in one event batch");
//		}
		
		if (!method_args.indices.isEmpty()) {
			QMetaObject::invokeMethod(args->table_model, "UpdateTable",
				Q_ARG(cornus::gui::UpdateTableArgs, method_args));
			method_args.indices.clear();
		}
	}
	
	if (close(epfd)) {
		mtl_status(errno);
	}
	
	return nullptr;
}
*/
SidePaneModel::SidePaneModel(cornus::App *app): app_(app)
{
	qRegisterMetaType<cornus::gui::UpdateSidePaneArgs>();
	qRegisterMetaType<cornus::gui::InsertArgs>();
}

SidePaneModel::~SidePaneModel()
{
}

QModelIndex
SidePaneModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}

int
SidePaneModel::rowCount(const QModelIndex &parent) const
{
	gui::SidePaneItems &items = app_->side_pane_items();
	MutexGuard guard(&items.mutex);
	return items.vec.size();
}

QVariant
SidePaneModel::data(const QModelIndex &index, int role) const
{
	if (index.column() != 0) {
		mtl_trace();
		return {};
	}
	
	if (role == Qt::TextAlignmentRole) {}
	
	const int row = index.row();
	gui::SidePaneItems &items = app_->side_pane_items();
	gui::SidePaneItem *item = items.vec[row];
	
	if (role == Qt::DisplayRole)
	{
		return item->table_name();
	} else if (role == Qt::FontRole) {
	} else if (role == Qt::BackgroundRole) {
	} else if (role == Qt::ForegroundRole) {
	} else if (role == Qt::DecorationRole) {
	}
	return {};
}

QVariant
SidePaneModel::headerData(int section_i, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (orientation == Qt::Horizontal)
		{
			return QString("Places");
		}
		return {};
	}
	return {};
}

void
SidePaneModel::InsertFromAnotherThread(cornus::gui::InsertArgs args)
{
	InsertRows(0, args.vec);
}

bool
SidePaneModel::InsertRows(const i32 at, const QVector<gui::SidePaneItem*> &items_to_add)
{
	{
		gui::SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		if (at > items.vec.size()) {
			mtl_trace();
			return false;
		}
	}
	const int first = at;
	const int last = at + items_to_add.size() - 1;
	beginInsertRows(QModelIndex(), first, last);

	{
		gui::SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		for (i32 i = 0; i < items_to_add.size(); i++)
		{
			auto *item = items_to_add[i];
			items.vec.insert(at + i, item);
		}
	}
	
	endInsertRows();
	return true;
}

bool
SidePaneModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (count <= 0)
		return false;
	
	CHECK_TRUE((count == 1));
	const int first = row;
	const int last = row + count - 1;
	
	beginRemoveRows(QModelIndex(), first, last);
	gui::SidePaneItems &items = app_->side_pane_items();
	MutexGuard guard(&items.mutex);
	auto &vec = items.vec;
	
	for (int i = count - 1; i >= 0; i--) {
		const i32 index = first + i;
		auto *item = vec[index];
		vec.erase(vec.begin() + index);
		delete item;
	}
	
	endRemoveRows();
	return true;
}

void SidePaneModel::UpdateIndices(const QVector<int> indices)
{
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
	
	if (min == -1 || max == -1) {
		//mtl_info("(-1) update range: %d", args.new_count);
		UpdateVisibleArea();
	} else {
		//mtl_info("update range min: %d, max: %d", min, max);
		UpdateRowRange(min, max);
	}
}

void
SidePaneModel::UpdateRange(int row1, int row2)
{
	int first, last;
	
	if (row1 > row2) {
		first = row2;
		last = row1;
	} else {
		first = row1;
		last = row2;
	}
	
	const QModelIndex top_left = createIndex(first, 0);
	const QModelIndex bottom_right = createIndex(last, 0);
	emit dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

void
SidePaneModel::UpdateTable(UpdateSidePaneArgs args)
{
	i32 added = args.new_count - args.prev_count;
	
	if (added > 0) {
		//mtl_info("added: %d", added - 1);
		beginInsertRows(QModelIndex(), 0, added - 1);
		endInsertRows();
	} else if (added < 0) {
		added = std::abs(added);
		beginRemoveRows(QModelIndex(), 0, added - 1);
		//mtl_info("removed: %d", added - 1);
		endRemoveRows();
	}
	
	UpdateIndices(args.indices);
}

void
SidePaneModel::UpdateVisibleArea() {
	QScrollBar *vs = side_pane_->verticalScrollBar();
	int row_start = side_pane_->rowAt(vs->value());
	int row_count = side_pane_->rowAt(side_pane_->height());
	UpdateRowRange(row_start, row_start + row_count);
}

}
