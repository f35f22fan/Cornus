#include "disks.hh"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../gui/SidePane.hpp"
#include "../gui/SidePaneItem.hpp"
#include "../gui/SidePaneModel.hpp"

namespace cornus::io::disks {

static const char *ServiceName = "org.freedesktop.UDisks2";

void* MountPartitionTh(void *p)
{
	pthread_detach(pthread_self());
	
	MountPartitionData *mps = (MountPartitionData*)p;
//	struct MountPartitionData {
//		gui::SidePaneItem *partition;
//		App *app;
//	};
	
	AutoDelete ad(mps->partition);
	AutoDelete mps_ad(mps);
	
	if (!QDBusConnection::systemBus().isConnected()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, QString("DBus: Failed to connect to system bus")));
		
		fprintf(stderr, "Cannot connect to the D-Bus system bus.\n"
		"To start it, run:\n\teval `dbus-launch --auto-syntax`\n");
		return nullptr;
	}
	
	QString object_name = QLatin1String("/org/freedesktop/UDisks2/block_devices/")
		+ mps->partition->GetPartitionName();
	auto object_name_ba = object_name.toLocal8Bit();
	
	QDBusInterface iface(ServiceName, object_name_ba.data(),
		"org.freedesktop.UDisks2.Filesystem", QDBusConnection::systemBus());
	
	if (!iface.isValid()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, QString("Invalid interface")));
		return nullptr;
	}
	
	QMap<QString, QVariant> args;
	QDBusReply<QString> reply = iface.call(QDBus::Block, "Mount", args);
	
	if (!reply.isValid()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, reply.error().message()));
		
		//fprintf(stderr, "Call failed: %s\n", qPrintable(reply.error().message()));
		return nullptr;
	}
	
//	PartitionEvent *pm = new PartitionEvent();
//	pm->type = PartitionEventType::Mount;
//	pm->mount_path = reply.value();
//	pm->dev_path = mps->partition->dev_path();
//	pm->fs = cornus::gui::sidepane::ReadMountedPartitionFS(pm->dev_path);
	
//	QMetaObject::invokeMethod(mps->app->side_pane(), "ReceivedPartitionEvent",
//		Q_ARG(cornus::PartitionEvent*, pm));
	
	return nullptr;
}

void* UnmountPartitionTh(void *p)
{
	pthread_detach(pthread_self());
	
	MountPartitionData *mps = (MountPartitionData*)p;
//	struct MountPartitionData {
//		gui::SidePaneItem *partition;
//		App *app;
//	};
	
	AutoDelete ad(mps->partition);
	AutoDelete mps_ad(mps);
	
	if (!QDBusConnection::systemBus().isConnected()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, QString("DBus: Failed to connect to system bus")));
		
		fprintf(stderr, "Cannot connect to the D-Bus system bus.\n"
		"To start it, run:\n\teval `dbus-launch --auto-syntax`\n");
		return nullptr;
	}
	
	QString object_name = QLatin1String("/org/freedesktop/UDisks2/block_devices/")
		+ mps->partition->GetPartitionName();
	auto object_name_ba = object_name.toLocal8Bit();
	
	QDBusInterface iface(ServiceName, object_name_ba.data(),
		"org.freedesktop.UDisks2.Filesystem", QDBusConnection::systemBus());
	
	if (!iface.isValid()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, QString("Invalid interface")));
		return nullptr;
	}
	
	QMap<QString, QVariant> args;
	args["bla"] = QVariant(1);
	QDBusReply<void> reply = iface.call(QDBus::Block, "Unmount", args);
	
	if (!reply.isValid()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearHasBeenClicked",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, reply.error().message()));
/// Unexpected reply signature: got "<empty signature>", expected "s" (QString)
		mtl_warn("Call failed: %s\n", qPrintable(reply.error().message()));
		return nullptr;
	}
	
//	PartitionEvent *pm = new PartitionEvent();
//	pm->type = PartitionEventType::Unmount;
//	pm->mount_path.clear();
//	pm->dev_path = mps->partition->dev_path();
	
//	QMetaObject::invokeMethod(mps->app->side_pane(), "ReceivedPartitionEvent",
//		Q_ARG(cornus::PartitionEvent*, pm));
	
	return nullptr;
}

}
