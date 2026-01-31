#include "sidepane.hh"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../ByteArray.hpp"
#include "../ElapsedTimer.hpp"
#include "../prefs.hh"
#include "TableModel.hpp"
#include "TreeItem.hpp"
#include "TreeModel.hpp"

#include <string.h>
#include <sys/epoll.h>

//#define CORNUS_PRINT_PARTITIONS_LOAD_TIME

namespace cornus::gui::sidepane {

static const QString dev_sd = QLatin1String("/dev/sd");
static const QString dev_nvm = QLatin1String("/dev/nvm");

DeviceAction DeviceActionFromStr(const QString &s)
{
	if (s == QLatin1String("add"))
		return DeviceAction::Added;
	if (s == QLatin1String("remove"))
		return DeviceAction::Removed;
	
	return DeviceAction::None;
}

Device DeviceFromStr(const QString &s)
{
	if (s == QLatin1String("partition"))
		return Device::Partition;
	if (s == QLatin1String("disk"))
		return Device::Disk;
	
	return Device::None;
}

int FindPlace(TreeItem *new_item, QVector<TreeItem*> &vec)
{
	for (int i = vec.size() - 1; i >= 0; i--) {
		TreeItem *next = vec[i];
		if (!SortItems(new_item, next)) {
			return i + 1;
		}
	}
	
	return 0;
}

TreeItem* FindPartitionByDevPath(const QString &dev_path,
	const QVector<TreeItem*> &vec)
{
	for (TreeItem *next: vec) {
		if (next->is_partition() && next->disk_info().dev_path == dev_path)
			return next;
	}
	
	return nullptr;
}

QString GetDevPath(GVolume *vol)
{
	QString dev_path = g_volume_get_identifier(vol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	if (dev_path.startsWith(dev_sd) || dev_path.startsWith(dev_nvm))
		return dev_path;
	
	return QString();
}

void LoadAllVolumes(QVector<TreeItem*> &vec)
{
	udev_list_partitions(vec);
	io::ReadParams read_params = {};
	ByteArray ba;
	MTL_CHECK_VOID(io::ReadFile(QLatin1String("/proc/mounts"), ba, read_params));
	QString data = ba.toString();
	QStringList lines = data.split('\n');
	
	for (const auto &line: lines)
	{
		auto tokens = line.split(' ');
		if (tokens.size() <= 1)
			continue;
		
		const QString dev_path = tokens[0];
		if (!io::valid_dev_path(dev_path))
			continue;
		const auto &mount_path = tokens[1];
		TreeItem *mounted_item = FindPartitionByDevPath(dev_path, vec);
		if (mounted_item) {
			mounted_item->mounted(true);
			mounted_item->mount_path(mount_path);
		}
	}
}

bool LoadBookmarks(QVector<TreeItem*> &vec)
{
	const QString full_path = prefs::GetBookmarksFilePath();
	io::ReadParams read_params = {};
	read_params.can_rely = CanRelyOnStatxSize::Yes;
	ByteArray buf;
	MTL_CHECK(io::ReadFile(full_path, buf, read_params));
	
	if (!buf.has_more())
		return false;
	
	u16 version = buf.next_u16();
	MTL_CHECK(version == prefs::BookmarksFormatVersion);
	
	while (buf.has_more()) {
		TreeItem *p = new TreeItem();
		p->type(TreeItemType(buf.next_u8()));
		p->mount_path(buf.next_string());
		p->bookmark_name(buf.next_string());
		vec.append(p);
	}
	
	return true;
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
	LoadAllVolumes(method_args.partitions);
#ifdef CORNUS_PRINT_PARTITIONS_LOAD_TIME
	ci64 mc = timer.elapsed_mc();
	Q_UNUSED(mc);
	mtl_info("Partitions load time: %ld mc", mc);
#endif
	
	LoadBookmarks(method_args.bookmarks);
	TreeData &tree_data = app->tree_data();
	{
		auto guard = tree_data.guard();
		while (!tree_data.widgets_created)
		{
			tree_data.CondWait();
		}
	}
	
	auto *tree_model = app->tree_model();
	QMetaObject::invokeMethod(tree_model, "InsertFromAnotherThread",
		Q_ARG(cornus::gui::InsertArgs, method_args));

	return nullptr;
}

void PrintDev(udev_device *dev)
{
	{
		struct udev_list_entry *first_entry = udev_device_get_properties_list_entry(dev);
		MTL_CHECK_VOID(first_entry != nullptr);
		struct udev_list_entry *next_entry;
		mtl_info("===================PROPERTIES:");
		udev_list_entry_foreach(next_entry, first_entry)
		{
			mtl_info(">> %s=\"%s\"",
				udev_list_entry_get_name(next_entry),
				udev_list_entry_get_value(next_entry));
		}
	}
	{
		struct udev_list_entry *first_entry = udev_device_get_tags_list_entry(dev);
		MTL_CHECK_VOID(first_entry != nullptr);
		struct udev_list_entry *next_entry;
		mtl_info("udev_device_get_tags_list_entry LIST:");
		udev_list_entry_foreach(next_entry, first_entry)
		{
			mtl_info(">> %s=\"%s\"",
				udev_list_entry_get_name(next_entry),
				udev_list_entry_get_value(next_entry));
		}
	}
}

void ReadDiskInfo(struct udev_device *device, io::DiskInfo &info)
{
	dev_t devn = udev_device_get_devnum(device);
	info.dev_num.major = major(devn);
	info.dev_num.minor = minor(devn);
	info.id_model = udev_device_get_property_value(device, "ID_MODEL");
	info.dev_path = udev_device_get_devnode(device);
}

bool SortItems(TreeItem *a, TreeItem *b) 
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
	
	PartitionInfo *pi8 = a->partition_info();
	PartitionInfo *pi16 = b->partition_info();
	if (pi8) {
		if (!pi16)
			return true;
	} else if (pi16) {
		return false;
	}
	cint i = io::CompareStrings(a->disk_info().dev_path, b->disk_info().dev_path);
	return (i >= 0) ? false : true;
}

void udev_list_partitions(QVector<TreeItem*> &vec)
{
	struct udev *udev = udev_new();
	MTL_CHECK_VOID(udev != nullptr);
	UdevAutoUnref auto_unref(udev);

	struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	MTL_CHECK_VOID(enumerate != nullptr);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_scan_devices(enumerate);

	/// fillup device list
	struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
	MTL_CHECK_VOID(devices != nullptr);
	const QString loop_str = QLatin1String("loop");
	const QString partition_str = QLatin1String("partition");
	const QString disk_str = QLatin1String("disk");
	struct udev_list_entry *next_entry;
	
	io::DiskInfo disk_info = {};
	
	udev_list_entry_foreach(next_entry, devices)
	{
		const char *path = udev_list_entry_get_name(next_entry);
		// mtl_info("dev path: %s", path);
		struct udev_device *device = udev_device_new_from_syspath(udev, path);
		UdevDeviceAutoUnref udau(device);
		const QString sys_name = udev_device_get_sysname(device);
		/// skip if device/disk is a loop device
		if (sys_name.startsWith(loop_str))
			continue;
		
		const QString dev_type = udev_device_get_devtype(device);
		cbool is_disk = (dev_type == disk_str);
		if (is_disk) {
			ReadDiskInfo(device, disk_info);
			if (disk_info.id_model.isEmpty()) {
				continue;
			}
			TreeItem *disk = TreeItem::NewDisk(disk_info, 0);
			if (disk) {
				vec.append(disk);
			}
		}
		
		if (!is_disk) { // don't read twice where disk and partition are one
			ReadDiskInfo(device, disk_info);
		}
		TreeItem *item = TreeItem::FromDevice(device, &disk_info);
		if (item) {
			vec.append(item);
		}
		
	}
	/* free enumerate */
	udev_enumerate_unref(enumerate);
}

void* monitor_devices(void *args)
{
	pthread_detach(pthread_self());
	
	App *app = (App*) args;
	struct udev *udev = udev_new();
	if (!udev) {
		return NULL;
	}
	UdevAutoUnref auto_unref_udev(udev);

	struct udev_monitor *monitor = udev_monitor_new_from_netlink(udev, "udev");
	UdevMonitorAutoUnref auto_unref_monitor(monitor);
	
	const char *subsys = "block";
	udev_monitor_filter_add_match_subsystem_devtype(monitor, subsys, "disk");
	udev_monitor_filter_add_match_subsystem_devtype(monitor, subsys, "partition");
	udev_monitor_enable_receiving(monitor);
	cint monitor_fd = udev_monitor_get_fd(monitor);
	cint epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		mtl_status(errno);
		return 0;
	}
	
	AutoCloseFd epoll_fd_(epoll_fd);
	cint app_quitting_fd = app->app_quitting_fd();
	
	QVector<struct epoll_event> evt_vec(2);
	evt_vec[0].events = EPOLLIN;
	evt_vec[0].data.fd = monitor_fd;
	evt_vec[1].events = EPOLLIN;
	evt_vec[1].data.fd = app_quitting_fd;
	
	for (struct epoll_event &evt: evt_vec)
	{
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, evt.data.fd, &evt))
		{
			mtl_status(errno);
			return nullptr;
		}
	}
	
	cint ms = 100 * 1000; // 100sec
	while (true)
	{
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
		
		if (num_fds == 0)
			continue;
		
		bool do_exit = false;
		for (int i = 0; i < num_fds; i++)
		{
			auto &evt = evt_vec[i];
			if (!(evt.events & EPOLLIN))
				continue;
			
			if (evt.data.fd == monitor_fd)
			{
				ReadDeviceEvent(monitor, app);
			} else if (evt.data.fd == app_quitting_fd) {
				do_exit = true;
				// must read 8 bytes:
				i64 n;
				if (read(evt.data.fd, &n, sizeof n) < sizeof n)
				{
					mtl_trace();
					return 0;
				}
			}
		}
		
		if (do_exit)
			break;
	}
	
	return NULL;
}

void ReadDeviceEvent(struct udev_monitor *monitor, App *app)
{
	struct udev_device *device = udev_monitor_receive_device(monitor);
	MTL_CHECK_VOID(device != NULL);
	
	UdevDeviceAutoUnref udau(device);
	QString sys_path = udev_device_get_syspath(device);
	auto *device_action_str = udev_device_get_action(device);
	//mtl_info("Device action: %s", device_action_str);
	const DeviceAction device_action = DeviceActionFromStr(device_action_str);
	const Device device_enum = DeviceFromStr(udev_device_get_devtype(device));
	if (device_enum != Device::None && device_action != DeviceAction::None)
	{
		const QString dev_path = udev_device_get_devnode(device);
		TreeModel *model = app->tree_model();
		QMetaObject::invokeMethod(model, "DeviceEvent",
			Q_ARG(const cornus::Device, device_enum),
			Q_ARG(const cornus::DeviceAction, device_action),
			Q_ARG(const QString, dev_path),
			Q_ARG(const QString, sys_path));
	}
}

}
