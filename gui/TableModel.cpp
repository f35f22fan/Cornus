#include "TableModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "Table.hpp"

#include <sys/epoll.h>
#include <QFont>
#include <QScrollBar>
#include <QTime>

//#define DEBUG_INOTIFY

namespace cornus::gui {

struct WatchArgs {
	i32 dir_id = 0;
	QString dir_path;
	cornus::gui::TableModel *table_model = nullptr;
};

const size_t kInotifyEventSize = sizeof (struct inotify_event);
const size_t kInotifyEventBufLen = 256 * (kInotifyEventSize + 16);

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

TableModel::TableModel(cornus::App *app): app_(app)
{
	qRegisterMetaType<cornus::gui::UpdateTableArgs>();
	
	if (notify_.fd == -1)
		notify_.fd = inotify_init();
}

TableModel::~TableModel()
{
	MutexGuard guard(&files_.mutex);
	for (auto *file: files_.vec)
		delete file;
	
	files_.vec.clear();
}

void
TableModel::DeleteSelectedFiles() {
	QItemSelectionModel *select = app_->table()->selectionModel();
	
	if (!select->hasSelection())
		return;
	
	QModelIndexList rows = select->selectedRows();
	QVector<io::File*> delete_files;
	{
		MutexGuard guard(&files_.mutex);
		
		for (QModelIndex &row: rows) {
			const int index = row.row();
			io::File *pass = files_.vec[index]->Clone();
			pass->files_ = nullptr;
			pass->dir_path(files_.dir_path);
			delete_files.append(pass);
		}
	}
	
	for (io::File *file: delete_files) {
		io::Delete(file);
		delete file;
	}
}

QModelIndex
TableModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}

int
TableModel::rowCount(const QModelIndex &parent) const
{
	MutexGuard guard(&files_.mutex);
	return files_.vec.size();
}

int
TableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	
	return int(Column::Count);
}

QVariant
TableModel::data(const QModelIndex &index, int role) const
{
	/**
	const Column col = static_cast<Column>(index.column());
	
	if (role == Qt::TextAlignmentRole) {}
	
	io::File *file = files_.vec[row];
	
	if (role == Qt::DisplayRole)
	{
		if (col == Column::FileName) {
			//return ColumnFileNameData(file);
		} else if (col == Column::Size) {
		} else if (col == Column::Icon) {
			
		}
	} else if (role == Qt::FontRole) {
	} else if (role == Qt::BackgroundRole) {
	} else if (role == Qt::ForegroundRole) {
	} else if (role == Qt::DecorationRole) {
	}
	*/
	return {};
}

QVariant
TableModel::headerData(int section_i, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (orientation == Qt::Horizontal)
		{
			const Column section = static_cast<Column>(section_i);
			
			switch (section) {
			case Column::Icon: return QString();
			case Column::FileName: return QLatin1String("Name");
			case Column::Size: return QLatin1String("Size");
			case Column::TimeCreated: return QLatin1String("Created");
			case Column::TimeModified: return QLatin1String("Modified");
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

bool
TableModel::InsertRows(const i32 at, const QVector<cornus::io::File*> &files_to_add)
{
	MutexGuard guard(&files_.mutex);
	
	if (files_.vec.isEmpty())
		return false;
	
	const int first = at;
	const int last = at + files_to_add.size() - 1;
	
	beginInsertRows(QModelIndex(), first, last);
	
	for (i32 i = 0; i < files_to_add.size(); i++)
	{
		auto *song = files_to_add[i];
		files_.vec.insert(at + i, song);
	}
	
	endInsertRows();
	
	return true;
}

bool TableModel::IsAt(const QString &dir_path) const
{
	QString old_dir_path;
	{
		MutexGuard guard(&files_.mutex);
		old_dir_path = files_.dir_path;
	}
	
	if (old_dir_path.isEmpty())
		return false;
	
	bool same;
	if ((io::SameFiles(dir_path, old_dir_path, same) == io::Err::Ok) && same) {
		return true;
	}
	
	return false;
}

bool
TableModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (count <= 0)
		return false;
	
	CHECK_TRUE((count == 1));
	const int first = row;
	const int last = row + count - 1;
	
	beginRemoveRows(QModelIndex(), first, last);
	MutexGuard guard(&files_.mutex);
	auto &vec = files_.vec;
	
	for (int i = count - 1; i >= 0; i--) {
		const i32 index = first + i;
		auto *item = vec[index];
		vec.erase(vec.begin() + index);
		delete item;
	}
	
	endRemoveRows();
	return true;
}

void
TableModel::SwitchTo(io::Files &new_files)
{
	int prev_count, new_count;
	{
		MutexGuard guard(&files_.mutex);
		prev_count = files_.vec.size();
		new_count = new_files.vec.size();
	}
	
	beginRemoveRows(QModelIndex(), 0, prev_count - 1);
	{
		MutexGuard guard(&files_.mutex);
		for (auto *file: files_.vec)
			delete file;
		files_.vec.clear();
	}
	endRemoveRows();
	
	beginInsertRows(QModelIndex(), 0, new_count - 1);
	i32 dir_id;
	{
		MutexGuard guard(&files_.mutex);
		files_.dir_id++;
		dir_id = files_.dir_id;
		files_.dir_path = new_files.dir_path;
		/// copying sorting order is logically wrong because it overwrites
		/// the existing one.
		files_.show_hidden_files = new_files.show_hidden_files;
		files_.vec.resize(new_count);
		for (int i = 0; i < new_count; i++) {
			io::File *file = new_files.vec[i];
			file->files_ = &files_;
			files_.vec[i] = file;
		}
		new_files.vec.clear();
	}
	endInsertRows();
	
	WatchArgs *args = new WatchArgs {
		.dir_id = dir_id,
		.dir_path = new_files.dir_path,
		.table_model = this,
	};
	
	pthread_t th;
	int status = pthread_create(&th, NULL, cornus::gui::WatchDir, args);
	if (status != 0) {
		mtl_printq(new_files.dir_path);
		mtl_status(status);
	}
}

void
TableModel::UpdateRange(int row1, Column c1, int row2, Column c2)
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
	emit dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

void
TableModel::UpdateTable(UpdateTableArgs args)
{
	{
		MutexGuard guard(&files_.mutex);
		if (args.dir_id != files_.dir_id)
			return;
	}
	
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
	
	int min = -1, max = -1;
	bool initialize = true;
	
	for (int next: args.indices) {
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
	
	if (!scroll_to_and_select_.isEmpty()) {
/** When the user renames a file from a file rename dialog you want after
 renaming to select the newly renamed file and scroll to it,
 but you can't do it from the
 rename dialog code because the tableview will only receive the new
file name from the inotify event which is processed in another thread.
Hence the table can't scroll to it (returns false) as long as it hasn't
received the new file name from the inotify thread. Which means that if
it returns true all jobs are done (rename event processed, table
found the new file, selected and scrolled to it).
 Plus at the end the string is cleared to avoid trying to do this
 for no reason.*/
		if (app_->table()->ScrollToAndSelect(scroll_to_and_select_))
			scroll_to_and_select_.clear();
	}
}

void
TableModel::UpdateVisibleArea() {
	gui::Table *table = app_->table();
	QScrollBar *vs = table->verticalScrollBar();
	int row_start = table->rowAt(vs->value());
	int row_count = table->rowAt(table->height());
	UpdateRowRange(row_start, row_start + row_count);
}

}
