#include "SidePaneItem.hpp"

#include "../io/File.hpp"
#include "../io/io.hh"
#include "../ByteArray.hpp"

namespace cornus::gui {

static const QString dev_sd = QLatin1String("/dev/sd");
static const QString dev_nvm = QLatin1String("/dev/nvm");

SidePaneItem*
SidePaneItem::NewBookmark(io::File &file)
{
	SidePaneItem *p = new SidePaneItem();
	p->type(SidePaneItemType::Bookmark);
	p->mount_path(file.is_dir_or_so() ? file.build_full_path()
		: file.dir_path());
	p->bookmark_name(file.name());
	p->Init();
	return p;
}

SidePaneItem*
SidePaneItem::NewPartition(GVolume *vol)
{
	QString dev_path = g_volume_get_identifier(vol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	mtl_printq2("dev_path: ", dev_path);
	if (!dev_path.startsWith(dev_sd) && !dev_path.startsWith(dev_nvm)) {
		return nullptr;
	}
	
	auto *p = new SidePaneItem();
	p->dev_path(dev_path);
	p->set_partition();
	
	GMount *mount = g_volume_get_mount(vol);
	p->mounted(mount != nullptr);
	if (mount != nullptr) {
		GFile *file = g_mount_get_root(mount);
		CHECK_PTR_NULL(file);
		const QString mount_path = g_file_get_path(file);
		g_object_unref(file);
		p->mount_path(mount_path);
		g_object_unref(mount);
	}
	p->Init();
	return p;
}

SidePaneItem*
SidePaneItem::NewPartitionFromDevPath(const QString &dev_path)
{
	auto *p = new SidePaneItem();
	p->dev_path(dev_path);
	p->set_partition();
	p->Init();
	
	return p;
}

SidePaneItem*
SidePaneItem::Clone()
{
	SidePaneItem *p = new SidePaneItem();
	p->dev_path_ = dev_path_;
	p->bookmark_name_ = bookmark_name_;
	p->mount_path_ = mount_path_;
	p->fs_ = fs_;
	p->size_ = size_;
	p->type_ = type_;
	p->bits_ = bits_;
	
	return p;
}

QString
SidePaneItem::DisplayString()
{
	if (is_bookmark()) {
		return bookmark_name_;
	}
	
	QString s = QChar(0x2654) + ' ';
	
	int index = dev_path_.lastIndexOf('/');
	if (index == -1) {
		mtl_trace();
		return s;
	}
	
	s += dev_path_.mid(index + 1) + ' ';
	
	if (!size_str_.isEmpty())
		s += size_str_ + ' ';
	
	if (!mounted()) {
		return s;
	}
	
	QString name;
	index = mount_path_.lastIndexOf('/');
	if (index >= 0 && mount_path_.size() > 1)
		name = mount_path_.mid(index + 1);
	else
		name = mount_path_;
	
	if (name.size() > 8) {
		s += name.mid(0, 8);
	} else {
		s += name;
	}
	
//	if (!fs_.isEmpty())
//		s += ' ' + fs_;
	
	return s;
}

QString
SidePaneItem::GetPartitionName() const
{
	int index = dev_path_.lastIndexOf('/');
	if (index == -1) {
		mtl_trace();
		return QString();
	}
	
	return dev_path_.mid(index + 1);
}

void
SidePaneItem::Init()
{
	if (is_partition()) {
		io::ReadPartitionInfo(dev_path_, size_, size_str_);
	}
}

}
