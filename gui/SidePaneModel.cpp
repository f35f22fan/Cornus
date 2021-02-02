#include "SidePaneModel.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "../prefs.hh"
#include "SidePane.hpp"
#include "SidePaneItem.hpp"

#include <sys/epoll.h>
#include <QFont>
#include <QScrollBar>
#include <QHeaderView>
#include <QTime>

//#define DEBUG_INOTIFY

namespace cornus::gui {
namespace sidepane {

bool
SortSidePanes(SidePaneItem *a, SidePaneItem *b) 
{
/** Note: this function MUST be implemented with strict weak ordering
  otherwise it randomly crashes (because of undefined behavior),
  more info here:
 https://stackoverflow.com/questions/979759/operator-and-strict-weak-ordering/981299#981299 */
	
	const int i = a->dev_path().compare(b->dev_path());
	return (i > 0) ? false : true;
}


void LoadBookmarks(QVector<SidePaneItem*> &vec)
{
	const QString full_path = prefs::QueryAppConfigPath() + '/'
		+ prefs::BookmarksFileName;
	
	ByteArray buf;
	if (io::ReadFile(full_path, buf) != io::Err::Ok)
		return;
	
	u16 version = buf.next_u16();
	CHECK_TRUE_VOID((version == prefs::BookmarksFormatVersion));
	
	while (buf.has_more()) {
		SidePaneItem *p = new SidePaneItem();
		p->type(SidePaneItemType(buf.next_u8()));
		p->mount_path(buf.next_string());
		p->bookmark_name(buf.next_string());
		p->Init();
		vec.append(p);
	}
}

void LoadDrivePartition(const QString &dir_path, const QString &name,
	QVector<SidePaneItem*> &vec)
{
/// Check if this partition hasn't been loaded previously as a mounted one:
	for (SidePaneItem *next: vec) {
		if (next->dev_path().endsWith(name)) {
			return;
		}
	}
	
	const QString dev_path = QLatin1String("/dev/") + name;
	SidePaneItem *p = new SidePaneItem();
	p->dev_path(dev_path);
	p->type(SidePaneItemType::Partition);
	p->mounted(false);
	p->Init();
	vec.append(p);
}

void LoadDrivePartitions(QString dir_path, QVector<SidePaneItem*> &vec)
{
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	QVector<QString> names;
	CHECK_TRUE_VOID((io::ListFileNames(dir_path, names) == io::Err::Ok));
	const QString sd_abc = QLatin1String("sd");
	
	for (const QString &name: names) {
		if (name.startsWith(sd_abc))
			LoadDrivePartition(dir_path, name, vec);
	}
	
	std::sort(vec.begin(), vec.end(), SortSidePanes);
}

void LoadUnmountedPartitions(QVector<SidePaneItem*> &vec)
{
	QVector<QString> names;
	const QString dir = QLatin1String("/sys/block/");
	CHECK_TRUE_VOID((io::ListFileNames(dir, names) == io::Err::Ok));
	const QString prefix = QLatin1String("sd");
	
	for (const QString &name: names) {
		if (name.startsWith(prefix)) {
			LoadDrivePartitions(dir + name, vec);
		}
	}
}

void* LoadItems(void *args)
{
	pthread_detach(pthread_self());
	cornus::App *app = (cornus::App*) args;
	ByteArray buf;
	if (io::ReadFile(QLatin1String("/proc/mounts"), buf) != io::Err::Ok)
		return nullptr;
	
/// mtl_info("Have read: %ld bytes", buf.size());
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	auto list = s.splitRef('\n');
	const QString prefix = QLatin1String("/dev/sd");
	const QString skip_mount = QLatin1String("/boot/");
	const QString skip_mount2 = QLatin1String("/home");
	InsertArgs method_args;
	
	for (auto &line: list)
	{
		if (!line.startsWith(prefix))
			continue;
		
		auto args = line.split(" ");
		QStringRef mount_path = args[1];
		
		if (mount_path.startsWith(skip_mount) || mount_path == skip_mount2)
			continue;
		
		auto *p = new gui::SidePaneItem();
		p->dev_path(args[0].toString());
		p->mount_path(mount_path.toString());
		p->mounted(true);
		p->fs(args[2].toString());
		p->type(gui::SidePaneItemType::Partition);
		p->Init();
		method_args.vec.append(p);
	}
	
	LoadUnmountedPartitions(method_args.vec);
	
	LoadBookmarks(method_args.vec);
	
	SidePaneItems &items = app->side_pane_items();
	items.Lock();
	while (!items.widgets_created) {
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

QString ReadMountedPartitionFS(const QString &dev_path)
{
	ByteArray buf;
	if (io::ReadFile(QLatin1String("/proc/mounts"), buf) != io::Err::Ok)
		return QString();
	
/// mtl_info("Have read: %ld bytes", buf.size());
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	auto list = s.splitRef('\n');
	
	for (auto &line: list)
	{
		if (!line.startsWith(dev_path))
			continue;
		
		auto args = line.split(" ");
		return args[2].toString();
//		auto *p = new gui::SidePaneItem();
//		p->dev_path(args[0].toString());
//		p->mount_path(args[1]);
//		p->mounted(true);
//		p->fs(args[2].toString());
//		p->type(gui::SidePaneItemType::Partition);
//		p->Init();
	}
	
	return QString();
}

} // sidepane::

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
	
//	static QIcon hard_drive_icon = QIcon::fromTheme("drive-harddisk");
	
	if (role == Qt::TextAlignmentRole) {
		return Qt::AlignLeft + Qt::AlignVCenter;
	}
	
	const int row = index.row();
	gui::SidePaneItems &items = app_->side_pane_items();
	gui::SidePaneItem *item = items.vec[row];
	
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
		QStyleOptionViewItem option = table_->option();
		if (item->is_partition())
			return option.palette.light();
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
SidePaneModel::FinishDropOperation(QVector<io::File*> *files_vec, int row)
{
	auto &items = app_->side_pane_items();
	beginInsertRows(QModelIndex(), 0, files_vec->size() - 1);
	{
		MutexGuard guard(&items.mutex);
		int add = 0;
		for (io::File *file: *files_vec) {
			auto *p = SidePaneItem::NewBookmark(*file);
			items.vec.insert(row + add, p);
			delete file;
			add++;
		}
	}
	endInsertRows();
	delete files_vec;
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

void
SidePaneModel::InsertFromAnotherThread(cornus::gui::InsertArgs args)
{
	static bool set_size = true;
	
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

void
SidePaneModel::MoveBookmarks(QStringList str_list, const QPoint &pos)
{
	if (str_list.isEmpty())
		return;
	const int y = pos.y();
	const int rh = table_->verticalHeader()->defaultSectionSize();
	int insert_at_row = table_->rowAt(y);
	
	if (y % rh >= (rh/2))
		insert_at_row++;
	
	QVector<SidePaneItem*> taken_items;
	
	auto &items = app_->side_pane_items();
	{
		MutexGuard guard(&items.mutex);
		int before_row_count = 0;
		const int count = items.vec.size();
		
		for (int i = count - 1; i >= 0; i--)
		{
			SidePaneItem *next = items.vec[i];
			
			if (!next->is_bookmark())
				continue;
			
			if (str_list.contains(next->bookmark_name())) {
				taken_items.append(next);
				items.vec.removeAt(i);
				
				if (i < insert_at_row)
					before_row_count++;
			}
		}
		
		int insert_at = insert_at_row - before_row_count;
		
		for (int i = taken_items.size() - 1; i >= 0; i--) {
			auto *next = taken_items[i];
			items.vec.insert(insert_at++, next);
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
	QScrollBar *vs = table_->verticalScrollBar();
	int row_start = table_->rowAt(vs->value());
	int row_count = table_->rowAt(table_->height());
	UpdateRowRange(row_start, row_start + row_count);
}

}
