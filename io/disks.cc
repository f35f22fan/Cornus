#include "disks.hh"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../gui/SidePane.hpp"
#include "../gui/SidePaneItem.hpp"

namespace cornus::io::disks {

static const char *ServiceName = "org.freedesktop.UDisks2";

void* MountPartitionTh(void *p)
{
	pthread_detach(pthread_self());
	
	MountPartitionStruct *mps = (MountPartitionStruct*)p;
	struct MountPartitionStruct {
		gui::SidePaneItem *partition;
		App *app;
	};
	
	
	AutoDelete ad(mps->partition);
	AutoDelete mps_ad(mps);
	
	if (!QDBusConnection::systemBus().isConnected()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearEventInProgress",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, QString("DBus: Failed to connect to system bus")));
		
		fprintf(stderr, "Cannot connect to the D-Bus system bus.\n"
		"To start it, run:\n\teval `dbus-launch --auto-syntax`\n");
		return nullptr;
	}
	
	QString object_name = QLatin1String("/org/freedesktop/UDisks2/block_devices/")
		+ mps->partition->GetDevName();
	auto object_name_ba = object_name.toLocal8Bit();
	
	QDBusInterface iface(ServiceName, object_name_ba.data(),
		"org.freedesktop.UDisks2.Filesystem", QDBusConnection::systemBus());
	
	CHECK_TRUE_NULL(iface.isValid());
	
	QMap<QString, QVariant> args;
	QDBusReply<QString> reply = iface.call(QDBus::Block, "Mount", args);
	
	if (!reply.isValid()) {
		QMetaObject::invokeMethod(mps->app->side_pane(), "ClearEventInProgress",
			Q_ARG(QString, mps->partition->dev_path()),
			Q_ARG(QString, reply.error().message()));
		
		//fprintf(stderr, "Call failed: %s\n", qPrintable(reply.error().message()));
		return nullptr;
	}
	
	PartitionEvent *pm = new PartitionEvent();
	pm->type = PartitionEventType::Mount;
	pm->mount_path = reply.value();
	pm->dev_path = mps->partition->dev_path();
	
	QMetaObject::invokeMethod(mps->app->side_pane(), "ReceivedPartitionEvent",
		Q_ARG(cornus::PartitionEvent*, pm));
	
	return nullptr;
}

}
