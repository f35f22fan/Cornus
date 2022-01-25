#pragma once

#include "decl.hxx"
#include "gui/decl.hxx"
#include "err.hpp"

#include <QSet>
#include <QVector>

namespace cornus {

class Hid {
public:
	Hid(App *app);
	virtual ~Hid();
	
	void HandleKeySelect(gui::Tab *tab, const VDirection vdir, const int key, const Qt::KeyboardModifiers modifiers);
	
	void HandleKeyShiftSelect(gui::Tab *tab, const VDirection vdir, const int key);
	
	void HandleMouseSelectionShift(gui::Tab *tab, const QPoint &pos,
		QSet<int> &indices);
	
	void HandleMouseSelectionCtrl(gui::Tab *tab, const QPoint &pos,
		QSet<int> *indices);
	
	void HandleMouseSelectionNoModif(gui::Tab *tab, const QPoint &pos,
		QSet<int> &indices, bool mouse_pressed,
		gui::ShiftSelect *shift_select = nullptr);
	
	void SelectFileByIndex(gui::Tab *tab, const int file_index, const DeselectOthers des);
	
private:
	NO_ASSIGN_COPY_MOVE(Hid);
	
	App *app_ = nullptr;
};

} // namespace
