#include "SidePaneItem.hpp"

#include "../io/io.hh"
#include "../ByteArray.hpp"

namespace cornus::gui {

SidePaneItem*
SidePaneItem::Clone()
{
	SidePaneItem *p = new SidePaneItem();
	p->table_name_ = table_name_;
	p->dev_path_ = dev_path_;
	p->bookmark_name_ = bookmark_name_;
	p->mount_path_ = mount_path_;
	p->fs_ = fs_;
	p->type_ = type_;
	p->selected_ = selected_;
	
	return p;
}

void
SidePaneItem::Init()
{
	if (is_partition())
		ReadSize();
	
	if (is_bookmark()) {
		table_name_ = bookmark_name_;
		return;
	}
	
	QString s;
	int index = dev_path_.lastIndexOf('/');
	
	QString name;
	index = mount_path_.lastIndexOf('/');
	if (index >= 0 && mount_path_.size() > 1)
		name = mount_path_.mid(index + 1);
	else
		name = mount_path_;
	
	if (name.size() > 10) {
		s += name.mid(0, 10);
	} else {
		s += name;
	}
	
	if (!size_str_.isEmpty())
		s += QLatin1String(" (") + size_str_ + ')';
	
	table_name_ = s;
}

void
SidePaneItem::ReadSize() {
	int index = dev_path_.indexOf(QLatin1String("/sd"));
	CHECK_TRUE_VOID((index != -1));
	QStringRef sda_no_number = dev_path_.midRef(index + 1, 3);
	QStringRef sda_with_number = dev_path_.midRef(index + 1);
	QString full_path = QLatin1String("/sys/block/") + sda_no_number
		+ '/' + sda_with_number + QLatin1String("/size");
	
	ByteArray buf;
	CHECK_TRUE_VOID((io::ReadFile(full_path, buf) == io::Err::Ok));
	QString s = QString::fromLocal8Bit(buf.data(), buf.size());
	bool ok;
	i64 num = s.toLong(&ok);
	if (ok) {
		size_ = num * 512;
		size_str_ = io::SizeToString(size_);
	}
}
}
