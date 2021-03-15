#include "ShallowItem.hpp"

#include "../io/io.hh"

namespace cornus::gui {

static void LoadDrivePartition(const QString &name, QVector<ShallowItem*> &vec)
{
	const QString dev_path = QLatin1String("/dev/") + name;
	ShallowItem *p = new ShallowItem();
	p->dev_path(dev_path);
	p->mounted(false);
	p->Init();
	if (p->size() == 0) {
		delete p;
	} else {
		vec.append(p);
	}
}

static void LoadDrivePartitions(QString drive_dir, const QString &drive_name,
	QVector<ShallowItem*> &vec)
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
}

ShallowItem::ShallowItem() {}

ShallowItem::~ShallowItem() {}

QString
ShallowItem::GetPartitionName() const
{
	int index = dev_path_.lastIndexOf('/');
	if (index == -1) {
		mtl_trace();
		return QString();
	}
	
	return dev_path_.mid(index + 1);
}

void ShallowItem::Init()
{
	io::ReadPartitionInfo(dev_path_, size_, size_str_);
}

void ShallowItem::LoadAllPartitions(QVector<ShallowItem*> &vec)
{
	QVector<QString> drive_names;
	const QString dir = QLatin1String("/sys/block/");
	CHECK_TRUE_VOID((io::ListFileNames(dir, drive_names, io::sd_nvme) == io::Err::Ok));
	for (const QString &drive_name: drive_names) {
		LoadDrivePartitions(dir + drive_name, drive_name, vec);
	}
}

}


