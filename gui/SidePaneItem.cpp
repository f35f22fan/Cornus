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
