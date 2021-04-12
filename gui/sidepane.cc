#include "sidepane.hh"

#include "../App.hpp"
#include "SidePaneModel.hpp"

namespace cornus::gui::sidepane {

bool SortItems(SidePaneItem *a, SidePaneItem *b) 
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
		if (!SortItems(new_item, next)) {
			return i + 1;
		}
	}
	
	return 0;
}

void LoadAllVolumes(QVector<SidePaneItem*> &vec)
{
	GVolumeMonitor *monitor = g_volume_monitor_get();
	GList *list = g_volume_monitor_get_volumes(monitor);
	auto next = list;
	const QString root_path = QLatin1String("/");
	SidePaneItem *root = nullptr;
	
	while (next)
	{
		GVolume *vol = (GVolume*)next->data;
		auto *item = SidePaneItem::NewPartition(vol);
		if (item != nullptr) {
			vec.append(item);
			if (root == nullptr && item->mount_path() == root_path) {
				root = item;
			}
		}
		g_object_unref(next->data);
		next = next->next;
	}
	
	g_list_free(list);
	g_object_unref(monitor);
	
	if (root != nullptr)
		return;
	
	// GVolumeMonitor doesn't list the root partition, load it by hand:
	ByteArray ba;
	CHECK_TRUE_VOID(io::ReadFile(QLatin1String("/proc/mounts"), ba));
	QString data = ba.toString();
	QVector<QStringRef> lines = data.splitRef('\n');
	for (const auto &line: lines) {
		auto parts = line.split(' ');
		if (root_path == parts[1]) {
			root = SidePaneItem::NewPartitionFromDevPath(parts[0].toString());
			root->mounted(true);
			root->mount_path(root_path);
			break;
		}
	}
	
	if (root != nullptr)
		vec.append(root);
}

void VolumeMounted(GVolumeMonitor *volume_monitor,
	GMount *mount, gpointer user_data)
{
	GVolume *vol = g_mount_get_volume(mount);
	QString dev_path = g_volume_get_identifier(vol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	g_object_unref(vol);
	GFile *file = g_mount_get_root(mount);
	CHECK_PTR_VOID(file);
	const QString mount_path = g_file_get_path(file);
	g_object_unref(file);
	
	App *app = (App*)user_data;
	gui::SidePaneModel *model = app->side_pane_model();
	SidePaneItems &items = app->side_pane_items();
	{
		auto guard = items.guard();
		for (SidePaneItem *next: items.vec) {
			if (next->dev_path() == dev_path) {
				next->mount_path(mount_path);
				next->mount_changed(true);
				next->mounted(true);
			}
		}
	}
	
	QMetaObject::invokeMethod(model, "PartitionsChanged");
}

void MountChanged(GVolumeMonitor *volume_monitor,
	GMount *mount, gpointer user_data)
{
	GFile *file = g_mount_get_root(mount);
	CHECK_PTR_VOID(file);
	const QString mount_path = g_file_get_path(file);
	Q_UNUSED(mount_path);
	g_object_unref(file);
	
	mtl_info("Mount changed: mount path: %s", qPrintable(mount_path));
	
	GVolume *vol = g_mount_get_volume(mount);
	if (vol) {
		QString dev_path = g_volume_get_identifier(vol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		g_object_unref(vol);
		mtl_info("dev_path: %s", qPrintable(dev_path));
	}
}

void VolumeUnmounted(GVolumeMonitor *volume_monitor,
	GMount *mount, gpointer user_data)
{
	App *app = (App*) user_data;
	Q_UNUSED(app);
	
	GFile *file = g_mount_get_root(mount);
	const QString mount_path = g_file_get_path(file);
	CHECK_PTR_VOID(file);
	mtl_info("Volume Unmounted: name: \"%s\", mount_path: \"%s\"",
		g_mount_get_name(mount),
		qPrintable(mount_path));
	g_object_unref(file);
	
	gui::SidePaneModel *model = app->side_pane_model();
	SidePaneItems &items = app->side_pane_items();
	{
		auto guard = items.guard();
		for (SidePaneItem *next: items.vec) {
			if (next->mounted()) {
				if (next->mount_path() == mount_path) {
					next->mount_changed(true);
					next->mounted(false);
					next->mount_path(QString());
				}
			}
		}
	}
	
	QMetaObject::invokeMethod(model, "PartitionsChanged");
}

}
