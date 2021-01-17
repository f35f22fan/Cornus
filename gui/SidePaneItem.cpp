#include "SidePaneItem.hpp"

namespace cornus::gui {

SidePaneItem*
SidePaneItem::Clone()
{
	SidePaneItem *p = new SidePaneItem();
	p->table_name_ = table_name_;
	p->dev_path_ = dev_path_;
	p->mount_path_ = mount_path_;
	p->fs_ = fs_;
	p->type_ = type_;
	
	return p;
}

void
SidePaneItem::Init()
{
	QString s;
	int index = dev_path_.lastIndexOf('/');
	
	if (index != -1)
		s = dev_path_.mid(index + 1);
	s.append(' ');
	
	index = mount_path_.lastIndexOf('/');
	if (index > 0)
		s += mount_path_.mid(index + 1);
	else
		s += mount_path_;
	
	table_name_ = s;
}

}
