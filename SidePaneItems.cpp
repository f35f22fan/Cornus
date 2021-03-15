#include "SidePaneItems.hpp"

#include "gui/SidePaneItem.hpp"

namespace cornus {

SidePaneItems::SidePaneItems() {}

SidePaneItems::~SidePaneItems()
{
	for (gui::SidePaneItem *next: vec) {
		delete next;
	}
}

}


