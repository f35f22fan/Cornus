#pragma once
//extern "C" {
#include <udisks/udisks.h>
#include <glib.h>
//}

#include "../decl.hxx"
#include "../io/decl.hxx"
#include "decl.hxx"
#include <libudev.h>

namespace cornus::gui::sidepane {

DeviceAction DeviceActionFromStr(const QString &s);
Device DeviceFromStr(const QString &s);

struct Item {
	QString dev_path;
	QString fs;
	QString mount_point;
};

int FindPlace(TreeItem *new_item, QVector<TreeItem*> &vec);
QString GetDevPath(GVolume *vol);
void LoadAllVolumes(QVector<TreeItem*> &vec);
bool LoadBookmarks(QVector<TreeItem*> &vec);
void* LoadItems(void *args);
void ReadDiskInfo(struct udev_device *device, io::DiskInfo &info);
bool SortItems(TreeItem *a, TreeItem *b);
void* monitor_devices(void *args);
void udev_list_partitions(QVector<TreeItem*> &vec);

}
