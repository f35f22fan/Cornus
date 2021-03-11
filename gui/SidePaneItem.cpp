#include "SidePaneItem.hpp"

#include "../io/File.hpp"
#include "../io/io.hh"
#include "../ByteArray.hpp"

namespace cornus::gui {

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
SidePaneItem::Clone()
{
	SidePaneItem *p = new SidePaneItem();
	p->dev_path_ = dev_path_;
	p->bookmark_name_ = bookmark_name_;
	p->mount_path_ = mount_path_;
	p->size_ = size_;
	p->fs_ = fs_;
	p->type_ = type_;
	p->bits_ = bits_;
	p->minor_ = minor_;
	p->major_ = major_;
	
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
	if (is_partition())
		ReadStats();
}

void
SidePaneItem::ReadStats()
{
///	mtl_printq2("dev path: ", dev_path_);
	int index = dev_path_.indexOf(QLatin1String("/sd"));
	QStringRef drive_name, partition_name;
	if (index != -1)
	{
		drive_name = dev_path_.midRef(index + 1, 3);
		partition_name = dev_path_.midRef(index + 1);
	} else {
		int start = dev_path_.indexOf(QLatin1String("/nvme"));
		if (start == -1)
			return;
		int end = dev_path_.lastIndexOf('p');
		if (end == -1)
			return;
		start++; /// skip '/'
		drive_name = dev_path_.midRef(start, end - start);
		partition_name = dev_path_.midRef(end);
	}
	
	QString full_path = QLatin1String("/sys/block/") + drive_name
		+ '/' + partition_name;
	
	QString size_path = full_path + QLatin1String("/size");
	
	ByteArray buf;
	CHECK_TRUE_VOID((io::ReadFile(size_path, buf) == io::Err::Ok));
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	bool ok;
	i64 num = s.toLong(&ok);
	if (ok) {
		size_ = num * 512;
		size_str_ = io::SizeToString(size_, true);
	}
	
	QString dev_path = full_path + QLatin1String("/dev");
	buf.to(0);
	CHECK_TRUE_VOID((io::ReadFile(dev_path, buf) == io::Err::Ok));
	s = buf.toString().trimmed();
	auto list = s.splitRef(':');
	i64 n = list[0].toLong(&ok);
	if (ok) {
		major_ = n;
	}
	n = list[1].toLong(&ok);
	if (ok) {
		minor_ = n;
	}
}

}
