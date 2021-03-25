#include "SidePaneModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../defines.hxx"
#include "../ElapsedTimer.hpp"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "../prefs.hh"
#include "../Prefs.hpp"
#include "SidePane.hpp"
#include "SidePaneItem.hpp"
#include "../SidePaneItems.hpp"

#include <sys/epoll.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QScrollBar>
#include <QHeaderView>
#include <QTime>
#include <sys/epoll.h>

namespace cornus::gui {
namespace sidepane {
const size_t kInotifyEventBufLen = 8 * (sizeof(struct inotify_event) + 16);

static bool SortSidePanes(SidePaneItem *a, SidePaneItem *b) 
{
/** Note: this function MUST be implemented with strict weak ordering
  otherwise it randomly crashes (because of undefined behavior),
  more info here:
 https://stackoverflow.com/questions/979759/operator-and-strict-weak-ordering/981299#981299 */
	
	if (a->is_partition()) {
		if (!b->is_partition())
			return true;
	} else if (b->is_partition())
		return false;
	
	const int i = io::CompareStrings(a->dev_path(), b->dev_path());
	return (i >= 0) ? false : true;
}

int FindPlace(SidePaneItem *new_item, QVector<SidePaneItem*> &vec)
{
	for (int i = vec.size() - 1; i >= 0; i--) {
		SidePaneItem *next = vec[i];
		if (!SortSidePanes(new_item, next)) {
			return i + 1;
		}
	}
	
	return 0;
}

static void ReadMountedPartitions(QVector<sidepane::Item> &v)
{
	ByteArray buf;
	if (io::ReadFile(QLatin1String("/proc/mounts"), buf) != io::Err::Ok)
		return;
	
	QString s = buf.toString();
	auto list = s.splitRef('\n');
	const QString dev_nvme = QLatin1String("/dev/nvme");
	const QString dev_sd = QLatin1String("/dev/sd");
	
	for (auto &line: list)
	{
		if (line.startsWith(dev_nvme) || line.startsWith(dev_sd))
		{
			auto args = line.split(' ');
			sidepane::Item item;
			item.dev_path = args[0].toString();
			item.mount_point = args[1].toString();
			item.fs = args[2].toString();
			v.append(item);
		}
	}
}

static void MarkMountedPartitions(QVector<SidePaneItem*> &vec)
{
	QVector<sidepane::Item> mounted_vec;
	ReadMountedPartitions(mounted_vec);
	
	for (SidePaneItem *item: vec)
	{
		if (!item->is_partition())
			continue;
		bool found = false;
		for (int row = 0; row < mounted_vec.size(); row++)
		{
			sidepane::Item &mounted = mounted_vec[row];
			if (mounted.dev_path == item->dev_path())
			{
				found = true;
				if (!item->mounted()) {
					item->mounted(true);
					item->mount_path(mounted.mount_point);
					item->fs(mounted.fs);
					item->mount_changed(true);
///					mtl_info("Made mounted %s", qPrintable(item->dev_path()));
				}
				mounted_vec.remove(row);
				break;
			}
		}
		
		if (!found && item->mounted()) {
			item->mounted(false);
			item->mount_changed(true);
///			mtl_info("Made unmounted %s", qPrintable(item->dev_path()));
		}
	}
}

static bool LoadBookmarks(QVector<SidePaneItem*> &vec)
{
	const QString full_path = prefs::GetBookmarksFilePath();
	
	ByteArray buf;
	if (io::ReadFile(full_path, buf) != io::Err::Ok)
		return false;
	
	if (!buf.has_more())
		return false;
	
	u16 version = buf.next_u16();
	CHECK_TRUE((version == prefs::BookmarksFormatVersion));
	
	while (buf.has_more()) {
		SidePaneItem *p = new SidePaneItem();
		p->type(SidePaneItemType(buf.next_u8()));
		p->mount_path(buf.next_string());
		p->bookmark_name(buf.next_string());
		p->Init();
		vec.append(p);
	}
	
	return true;
}

void LoadDrivePartition(const QString &name, QVector<SidePaneItem*> &vec)
{
	const QString dev_path = QLatin1String("/dev/") + name;
	SidePaneItem *p = new SidePaneItem();
	p->dev_path(dev_path);
	p->set_partition();
	p->mounted(false);
	p->Init();
	const auto _32MiB = 32 * 1024 * 1024;
	if (p->size() < _32MiB) {
		delete p;
	} else {
		vec.append(p);
	}
}

static void LoadDrivePartitions(QString drive_dir,
	const QString &drive_name, QVector<SidePaneItem*> &vec)
{
	if (!drive_dir.endsWith('/'))
		drive_dir.append('/');
	
	QVector<QString> names;
	
	CHECK_TRUE_VOID((io::ListFileNames(drive_dir, names, io::sd_nvme) == io::Err::Ok));
	
	for (const QString &filename: names) {
		LoadDrivePartition(filename, vec);
	}
	
	if (names.isEmpty())
		LoadDrivePartition(drive_name, vec);
	
	std::sort(vec.begin(), vec.end(), SortSidePanes);
}

static void LoadAllPartitions(QVector<SidePaneItem*> &vec)
{
	QVector<QString> drive_names;
	const QString dir = QLatin1String("/sys/block/");
	CHECK_TRUE_VOID((io::ListFileNames(dir, drive_names, io::sd_nvme) == io::Err::Ok));
	for (const QString &drive_name: drive_names) {
		LoadDrivePartitions(dir + drive_name, drive_name, vec);
	}
}

void* LoadItems(void *args)
{
	pthread_detach(pthread_self());
#ifdef CORNUS_PRINT_PARTITIONS_LOAD_TIME
	ElapsedTimer timer;
	timer.Continue();
#endif
	cornus::App *app = (cornus::App*) args;
	InsertArgs method_args;
	LoadAllPartitions(method_args.vec);
#ifdef CORNUS_PRINT_PARTITIONS_LOAD_TIME
	const i64 mc = timer.elapsed_mc();
	mtl_info("Directly: %ldmc", mc);
#endif
	
	LoadBookmarks(method_args.vec);
	SidePaneItems &items = app->side_pane_items();
	items.Lock();
	while (!items.widgets_created)
	{
		int status = pthread_cond_wait(&items.cond, &items.mutex);
		if (status != 0) {
			mtl_warn("pthread_cond_wait: %s", strerror(status));
			break;
		}
	}
	items.Unlock();
	
	QMetaObject::invokeMethod(app->side_pane_model(),
		"InsertFromAnotherThread",
		Q_ARG(cornus::gui::InsertArgs, method_args));
	
	return nullptr;
}

void ComparePartitions(SidePaneModel *model, SidePaneItems &items,
	QVector<SidePaneItem*> &shallow_items)
{
	QVector<SidePaneItem*> &vec = items.vec;
	for (SidePaneItem *shallow_item: shallow_items)
	{
		bool contains = false;
		for (SidePaneItem *item: vec)
		{
			if (item->dev_path() == shallow_item->dev_path()) {
				contains = true;
				if (shallow_item->mounted() != item->mounted()) {
///					mtl_info("mount changed for %s", qPrintable(item->dev_path()));
					item->mount_changed(true);
					item->mounted(shallow_item->mounted());
					if (shallow_item->mounted())
						item->mount_path(shallow_item->mount_path());
					items.Unlock();
					QMetaObject::invokeMethod(model, "PartitionsChanged",
						Qt::BlockingQueuedConnection);
					items.Lock();
				}
				break;
			}
		}
		if (!contains) {
			SidePaneItem *new_item = shallow_item->Clone();
			int insert_at = FindPlace(new_item, vec);
			const bool destroyed = items.sidepane_model_destroyed;
			if (destroyed) {
				return;
			}
			items.Unlock();
			QMetaObject::invokeMethod(model, "PartitionAdded",
				Qt::BlockingQueuedConnection,
				Q_ARG(const int, insert_at), Q_ARG(SidePaneItem*, new_item));
			items.Lock();
		}
	}

	for (int i = vec.size() - 1; i >= 0; i--)
	{
		SidePaneItem *item = vec[i];
		if (!item->is_partition())
			continue;
		bool contains = false;
		for (SidePaneItem *shallow_item: shallow_items) {
			if (item->dev_path() == shallow_item->dev_path()) {
				contains = true;
				break;
			}
		}
		if (!contains) {
			if (items.sidepane_model_destroyed) {
				return;
			}
			items.Unlock();
			QMetaObject::invokeMethod(model, "PartitionRemoved",
				Qt::BlockingQueuedConnection, Q_ARG(int, i));
			items.Lock();
		}
	}
}

void ReadBookmarksFileEvent(int inotify_fd, char *buf,
	gui::SidePaneModel *model)
{
	const ssize_t num_read = read(inotify_fd, buf, kInotifyEventBufLen);
	i64 add = 0;
	QString bookmarks_file_name = prefs::GetBookmarksFileName();
	bool is_bookmarks_file = false;
	
	for (char *p = buf; p < buf + num_read; p += add) {
		struct inotify_event *ev = (struct inotify_event*) p;
		add = sizeof(struct inotify_event) + ev->len;
		if (!is_bookmarks_file) {
			is_bookmarks_file = (bookmarks_file_name == ev->name);
		}
	}
	
	if (!is_bookmarks_file)
		return;
	
	SidePaneItems &items = model->app()->side_pane_items();
	{
		/// If this app did it then there's no need to update
		auto guard = items.guard();
		if (items.bookmarks_changed_by_me) {
			items.bookmarks_changed_by_me = false;
			return;
		}
	}
	
	QVector<SidePaneItem*> new_items;
	CHECK_TRUE_VOID(LoadBookmarks(new_items));
	
	{
		items.Lock();
		if (items.sidepane_model_destroyed) {
			for (auto *next: new_items) {
				delete next;
			}
			items.Unlock();
			return;
		}
		
		items.Unlock();
		QMetaObject::invokeMethod(model, "SetBookmarks",
		Qt::BlockingQueuedConnection,
			Q_ARG(QVector<cornus::gui::SidePaneItem*>, new_items));
	}
	
	///QMetaObject::invokeMethod(model, "UpdateVisibleArea");
}

void* MonitorBookmarksAndPartitions(void *void_args)
{
	pthread_detach(pthread_self());
	
	SidePaneModel *model = (SidePaneModel*)void_args;
	SidePaneItems &items = model->app()->side_pane_items();
	io::Notify &notify = model->notify();
	
	char *buf = new char[kInotifyEventBufLen];
	AutoDeleteArr ad_arr(buf);
	const auto EventTypes = IN_CLOSE_WRITE;
	auto bookmarks_file_path = prefs::QueryAppConfigPath().toLocal8Bit();
	int wd = inotify_add_watch(notify.fd, bookmarks_file_path.data(), EventTypes);
	int epfd = epoll_create(1);
	
	if (wd != -1) {
		if (epfd == -1) {
			mtl_status(errno);
			return 0;
		}
		
		struct epoll_event pev = {};
		pev.events = EPOLLIN;
		pev.data.fd = notify.fd;
		
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify.fd, &pev)) {
			mtl_status(errno);
			close(epfd);
			return nullptr;
		}
	}
	
	struct epoll_event poll_event;
	const int seconds = 2;
	
	while (true)
	{
#ifdef PRINT_LOAD_TIME
		ElapsedTimer timer;
		timer.Continue();
#endif
		QVector<SidePaneItem*> new_items;
		LoadAllPartitions(new_items);
		bool repaint = false;
		MarkMountedPartitions(new_items);
		{
			auto guard = items.guard();
			while (!items.partitions_loaded) {
				int status = pthread_cond_wait(&items.cond, &items.mutex);
				if (status != 0) {
					mtl_status(status);
					break;
				}
			}
			if (items.sidepane_model_destroyed)
				break;
			ComparePartitions(model, items, new_items);
			for (SidePaneItem *next: new_items) {
				delete next;
			}
			
			if (items.sidepane_model_destroyed) {
				break;
			}
		}
#ifdef PRINT_LOAD_TIME
		mtl_info("Checked in %ldmc", timer.elapsed_mc());
#endif
		if (repaint) {
			QMetaObject::invokeMethod(model, "PartitionsChanged");
		}
		
		if (wd != -1) {
			int event_count = epoll_wait(epfd, &poll_event, 1, seconds * 1000);
			
			if (event_count > 0) {
				ReadBookmarksFileEvent(poll_event.data.fd, buf, model);
			}
		} else {
			sleep(seconds);
		}
	}
	
	if (wd != -1) {
		if (close(epfd)) {
			mtl_status(errno);
		}
	}
	
	return nullptr;
}

} // sidepane::
SidePaneModel::SidePaneModel(cornus::App *app): app_(app)
{
	qRegisterMetaType<cornus::gui::UpdateSidePaneArgs>();
	qRegisterMetaType<cornus::gui::InsertArgs>();
	notify_.Init();
	
	pthread_t th;
	int status = pthread_create(&th, NULL, sidepane::MonitorBookmarksAndPartitions, this);
	if (status != 0)
		mtl_status(status);
}

SidePaneModel::~SidePaneModel()
{}

QModelIndex
SidePaneModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}

QVariant
SidePaneModel::data(const QModelIndex &index, int role) const
{
	if (index.column() != 0) {
		mtl_trace();
		return {};
	}
	
	if (role == Qt::TextAlignmentRole) {
		return Qt::AlignLeft + Qt::AlignVCenter;
	}
	
	const int row = index.row();
	SidePaneItems &items = app_->side_pane_items();
	gui::SidePaneItem *item = nullptr;
	{
		auto guard = items.guard();
		item = items.vec[row]->Clone();
	}
	AutoDelete ad(item);
	
	if (role == Qt::DisplayRole)
	{
		return item->DisplayString();
	} else if (role == Qt::FontRole) {
		QStyleOptionViewItem option = table_->option();
		if (item->selected() && item->is_partition()) {
			QFont f = option.font;
			f.setBold(true);
			return f;
		}
	} else if (role == Qt::BackgroundRole) {
		
//		if (item->has_been_clicked())
//			return QColor(255, 190, 190);
		
		if (row == table_->mouse_over_item_at()) {
			QColor c = table_->option().palette.highlight().color();
			return app_->hover_bg_color_gray(c);
		}
		if (item->selected() && item->is_partition()) {
			return table_->option().palette.highlight().color();
		}
	} else if (role == Qt::ForegroundRole) {
		if (item->is_partition() && !item->mounted()) {
			const int c = 110;
			return QColor(c, c, c);
		}
	} else if (role == Qt::DecorationRole) {
//		if (item->is_partition())
//			return hard_drive_icon;
	}
	return {};
}

void
SidePaneModel::DeleteSelectedBookmarks()
{
	int count = table_->GetSelectedBookmarkCount();
	
	if (count <= 0)
		return;
	
	beginRemoveRows(QModelIndex(), 0, count - 1);
	{
		auto &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		int item_count = items.vec.size();
		for (int i = item_count - 1; i >= 0; i--)
		{
			SidePaneItem *next = items.vec[i];
			if (next->is_bookmark() && next->selected()) {
				items.vec.remove(i);
				delete next;
			}
		}
	}
	endRemoveRows();
	UpdateVisibleArea();
	app_->SaveBookmarks();
}

void
SidePaneModel::FinishDropOperation(QVector<io::File*> *files_vec, const int row)
{
	auto &items = app_->side_pane_items();
	AutoDeleteVecP advp(files_vec);
	
	bool file_io = QApplication::keyboardModifiers() & Qt::ShiftModifier;
	for (io::File *file: *files_vec) {
		if (!file->is_dir_or_so()) {
			file_io = true;
			break;
		}
	}
	
	if (file_io) {
		QString to_dir;
		{
			MutexGuard guard = items.guard();
			if (row >= items.vec.size() - 1) {
				return;
			}
			gui::SidePaneItem *item = items.vec[row];
			to_dir = item->mount_path();
		}
		
		io::socket::MsgBits io_operation = io::socket::MsgBits::Copy;
		auto *ba = new ByteArray();
		ba->set_msg_id(io_operation);
		ba->add_string(to_dir);
		ba->add_i32(files_vec->size());
		
		for (io::File *next: *files_vec) {
			ba->add_string(next->build_full_path());
		}
		
		io::socket::SendAsync(ba);
		return;
	}
	
	beginInsertRows(QModelIndex(), 0, files_vec->size() - 1);
	{
		MutexGuard guard = items.guard();
		int added = 0;
		for (int i = files_vec->size() - 1; i >= 0; i--) {
			io::File *file = (*files_vec)[i];
			if (file->is_dir_or_so()) {
				auto *p = SidePaneItem::NewBookmark(*file);
				items.vec.insert(row + added, p);
				files_vec->remove(i);
				delete file;
				added++;
			}
		}
		cached_row_count_ += added;
	}
	endInsertRows();
	
	app_->SaveBookmarks();
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

void SidePaneModel::PartitionAdded(const int index, SidePaneItem *p)
{
	SidePaneItems &items = app_->side_pane_items();
	beginInsertRows(QModelIndex(), index, index);
	items.Lock();
	items.vec.insert(index, p);
	items.Unlock();
	cached_row_count_++;
	endInsertRows();
}
void SidePaneModel::PartitionRemoved(const int index)
{
	SidePaneItems &items = app_->side_pane_items();
	beginRemoveRows(QModelIndex(), index, index);
	items.Lock();
	delete items.vec[index];
	items.vec.remove(index);
	items.Unlock();
	cached_row_count_--;
	endRemoveRows();
}
void SidePaneModel::PartitionsChanged()
{
	SidePaneItems &items = app_->side_pane_items();
	QString mount_path;
	QString unmount_path;
	{
		MutexGuard guard(&items.mutex);
		for (SidePaneItem *item: items.vec) {
			if (!item->is_partition())
				continue;
			if (item->mount_changed() && item->has_been_clicked()) {
				item->has_been_clicked(false);
				if (item->mounted()) {
					mount_path = item->mount_path();
				} else {
					item->selected(false);
					unmount_path = item->mount_path();
				}
				break;
			}
		}
	}

	if (!mount_path.isEmpty()) {
		app_->GoTo(Action::To, {mount_path, Processed::No});
	} else if (!unmount_path.isEmpty()) {
		if (app_->current_dir().startsWith(unmount_path)) {
			app_->GoHome();
		}
	}
	UpdateVisibleArea();
}

void
SidePaneModel::InsertFromAnotherThread(cornus::gui::InsertArgs args)
{
	static bool set_size = true;
	
	if (set_size) {
		if (app_->prefs().splitter_sizes().size() > 0)
			set_size = false;
	}
	
	if (set_size) {
		set_size = false;
		auto *splitter = app_->main_splitter();
		QFont font = table_->option().font;
		font.setBold(true);
		QFontMetrics fm(font);
		int widest = 0;
		for (SidePaneItem *next: args.vec) {
			int n = fm.boundingRect(next->DisplayString()).width();
			if (n > widest) {
				widest = n;
			}
		}
		
		widest += 14;
		splitter->setSizes({widest, 1000});
	}
	
	InsertRows(0, args.vec);
	SidePaneItems &items = app_->side_pane_items();
	{
		MutexGuard guard(&items.mutex);
		items.partitions_loaded = true;
		int status = pthread_cond_broadcast(&items.cond);
		if (status != 0)
			mtl_status(status);
	}
}

bool
SidePaneModel::InsertRows(const i32 at, const QVector<gui::SidePaneItem*> &items_to_add)
{
	{
		SidePaneItems &items = app_->side_pane_items();
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
		SidePaneItems &items = app_->side_pane_items();
		MutexGuard guard(&items.mutex);
		for (i32 i = 0; i < items_to_add.size(); i++)
		{
			auto *item = items_to_add[i];
			items.vec.insert(at + i, item);
		}
	}
	cached_row_count_ += items_to_add.size();
	endInsertRows();
	return true;
}

void
SidePaneModel::MoveBookmarks(QStringList str_list, const QPoint &pos)
{
	if (str_list.isEmpty())
		return;
	
	const int rh = table_->verticalHeader()->defaultSectionSize();
	int insert_at_row = table_->rowAt(pos.y() + rh / 2);
	auto &items = app_->side_pane_items();
	{
		MutexGuard guard(&items.mutex);
		auto &vec = items.vec;
		
		if (insert_at_row == -1 || insert_at_row >= vec.size()) {
			insert_at_row = vec.size();
		}
		
		QVector<SidePaneItem*> taken_items;

		int before_row_count = 0;
		const int count = vec.size();
		
		for (int i = count - 1; i >= 0; i--)
		{
			SidePaneItem *next = items.vec[i];
			
			if (!next->is_bookmark())
				continue;
			
			if (str_list.contains(next->bookmark_name())) {
				taken_items.append(next);
				items.vec.removeAt(i);
				
				if (i <= insert_at_row)
					before_row_count++;
			}
		}
		int insert_at = insert_at_row - before_row_count;
		for (int i = taken_items.size() - 1; i >= 0; i--) {
			auto *next = taken_items[i];
			items.vec.insert(insert_at, next);
			insert_at++;
		}
	}
	
	UpdateVisibleArea();
	app_->SaveBookmarks();
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
	SidePaneItems &items = app_->side_pane_items();
	MutexGuard guard(&items.mutex);
	auto &vec = items.vec;
	
	for (int i = count - 1; i >= 0; i--) {
		const i32 index = first + i;
		auto *item = vec[index];
		vec.erase(vec.begin() + index);
		delete item;
	}
	cached_row_count_ -= count;
	endRemoveRows();
	return true;
}

void
SidePaneModel::SetBookmarks(QVector<SidePaneItem*> new_items)
{
	auto &items = app_->side_pane_items();
	int old_bookmark_count = 0;
	{
		auto guard = items.guard();
		for (SidePaneItem *next: items.vec) {
			if (next->is_bookmark()) {
				old_bookmark_count++;
			}
		}
	}
	beginRemoveRows(QModelIndex(), 0, old_bookmark_count - 1);
	{
		auto guard = items.guard();
		for (int i = items.vec.size() - 1; i >= 0; i--) {
			SidePaneItem *item = items.vec[i];
			if (item->is_bookmark()) {
				delete item;
				items.vec.remove(i);
			}
		}
		cached_row_count_ = items.vec.size();
	}
	endRemoveRows();
	
	beginInsertRows(QModelIndex(), 0, new_items.size() - 1);
	{
		auto guard = items.guard();
		for (SidePaneItem *new_item: new_items) {
			items.vec.append(new_item);
		}
		cached_row_count_ = items.vec.size();
	}
	endInsertRows();
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
SidePaneModel::UpdateVisibleArea() {
	QScrollBar *vs = table_->verticalScrollBar();
	int row_start = table_->rowAt(vs->value());
	int row_count = table_->rowAt(table_->height());
	UpdateRowRange(row_start, row_start + row_count);
}

}
