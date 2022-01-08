#pragma once

#include "decl.hxx"
#include "gui/decl.hxx"
#include "err.hpp"

#include <QVector>

namespace cornus {

class Hid {
public:
	Hid(App *app);
	virtual ~Hid();
	
	void HandleMouseSelectionCtrl(gui::Tab *tab, const QPoint &pos,
		QVector<int> *indices);
	
	void HandleMouseSelectionNoModif(gui::Tab *tab, const QPoint &pos,
		QVector<int> &indices, bool mouse_pressed,
		gui::ShiftSelect *shift_select = nullptr);
	
	void SelectFileByIndex(gui::Tab *tab, const int file_index, const gui::DeselectOthers des);
	
private:
	NO_ASSIGN_COPY_MOVE(Hid);
	
	App *app_ = nullptr;
};

} // namespace
