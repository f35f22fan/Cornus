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
	p->fs_ = fs_;
	p->type_ = type_;
	p->selected_ = selected_;
	
	return p;
}

QString
SidePaneItem::DisplayString()
{
	if (is_bookmark()) {
		return bookmark_name_;
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
	
	return s;
}

void
SidePaneItem::Init()
{
	if (is_partition())
		ReadSize();
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
