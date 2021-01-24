#pragma once

#include <QtCore/QCoreApplication>
#include <QtDBus/QtDBus>

#include "../decl.hxx"
#include "../err.hpp"
#include "../gui/decl.hxx"

namespace cornus::io::disks {

struct MountPartitionStruct {
	gui::SidePaneItem *partition = nullptr;
	App *app = nullptr;
};

void* MountPartitionTh(void *p);

}
