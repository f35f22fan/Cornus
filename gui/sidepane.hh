#pragma once
extern "C" {
#include <udisks/udisks.h>
#include <glib.h>
}

#include "decl.hxx"

namespace cornus::gui::sidepane {

int FindPlace(SidePaneItem *new_item, QVector<SidePaneItem*> &vec);
void LoadAllVolumes(QVector<SidePaneItem*> &vec);
bool SortItems(SidePaneItem *a, SidePaneItem *b);

void VolumeMounted(GVolumeMonitor *volume_monitor, GMount *mount, gpointer user_data);
void MountChanged(GVolumeMonitor *volume_monitor, GMount *mount, gpointer user_data);
void VolumeUnmounted(GVolumeMonitor *volume_monitor, GMount *mount, gpointer user_data);

}
