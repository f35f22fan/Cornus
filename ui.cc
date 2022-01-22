#include "ui.hh"

namespace cornus::ui {

DndType GetDndType(const QMimeData *md)
{
	if (!md)
		return DndType::None;
	
	if (md->hasUrls())
		return DndType::Urls;
	
	const QString dbus_service_key = QLatin1String("application/x-kde-ark-dndextract-service");
	const QString dbus_path_key = QLatin1String("application/x-kde-ark-dndextract-path");
	
	if (md->hasFormat(dbus_path_key) && md->hasFormat(dbus_service_key))
		return DndType::Ark;
	
	return DndType::None;
}

}
