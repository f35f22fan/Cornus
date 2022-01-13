#include "Hid.hpp"

#include "App.hpp"
#include "gui/IconView.hpp"
#include "gui/Tab.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "io/File.hpp"

namespace cornus {

Hid::Hid(App *app) : app_(app)
{
	Q_UNUSED(app_);
}

Hid::~Hid()
{}

void Hid::HandleMouseSelectionShift(gui::Tab *tab, const QPoint &pos,
	QVector<int> &indices)
{
	io::Files &files = tab->view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	int file_index = -1;
	gui::ShiftSelect *shift_select = nullptr;
	if (tab->view_mode() == gui::ViewMode::Details)
	{
		file_index = tab->table()->IsOnFileName_NoLock(pos, &file);
		shift_select = tab->table()->shift_select();
	} else {
		file = tab->icon_view()->GetFileAt_NoLock(pos, Clone::No, &file_index);
		shift_select = tab->icon_view()->shift_select();
	}
	
	bool on_file = file_index != -1;
	if (on_file)
	{
		if (shift_select->base_row == -1)
		{
			shift_select->base_row = file_index;
			file->set_selected(true);
			indices.append(file_index);
		} else {
			files.SelectAllFiles_NoLock(Selected::No, indices);
			files.SelectFileRange_NoLock(shift_select->base_row, file_index, indices);
		}
	}
}

void Hid::HandleMouseSelectionCtrl(gui::Tab *tab, const QPoint &pos,
	QVector<int> *indices)
{
	const gui::ViewMode view_mode = tab->view_mode();
	io::Files &files = tab->view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	int row = -1;
	if (view_mode == gui::ViewMode::Details)
	{
		row = tab->table()->IsOnFileName_NoLock(pos, &file);
	} else if (view_mode == gui::ViewMode::Icons) {
		file = tab->icon_view()->GetFileAt_NoLock(pos, Clone::No, &row);
	}
	
	if (file != nullptr)
	{
		file->toggle_selected();
		if (indices != nullptr && row != -1)
			indices->append(row);
	}
}

void Hid::HandleMouseSelectionNoModif(gui::Tab *tab, const QPoint &pos, QVector<int> &indices,
	bool mouse_pressed, gui::ShiftSelect *shift_select)
{
	const gui::ViewMode view_mode = tab->view_mode();
	io::Files &files = tab->view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	int file_index = -1;
	if (view_mode == gui::ViewMode::Details)
	{
		file_index = tab->table()->IsOnFileName_NoLock(pos, &file);
	} else if (view_mode == gui::ViewMode::Icons) {
		file = tab->icon_view()->GetFileAt_NoLock(pos, Clone::No, &file_index);
	}
	
	if (shift_select)
		shift_select->base_row = file_index;
	
	if (file_index != -1)
	{
		if (mouse_pressed)
		{
			if (file == nullptr || !file->is_selected())
				files.SelectAllFiles_NoLock(Selected::No, indices);
			if (file != nullptr && !file->is_selected() && file_index != -1) {
				file->selected(Selected::Yes);
				indices.append(file_index);
			}
		}
	} else {
		files.SelectAllFiles_NoLock(Selected::No, indices);
	}
}

void Hid::SelectFileByIndex(gui::Tab *tab, const int file_index,
	const DeselectOthers des)
{
	io::Files &files = tab->view_files();
	QVector<int> indices;
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		if (des == DeselectOthers::Yes)
		{
			int i = 0;
			for (io::File *next: vec)
			{
				if (next->is_selected())
				{
					next->toggle_selected();
					indices.append(i);
				}
				i++;
			}
		}
		
		if (file_index >= 0 && file_index < vec.size())
		{
			vec[file_index]->selected(Selected::Yes);
			indices.append(file_index);
		}
	}
	
	const gui::ViewMode view_mode = tab->view_mode();
	if (view_mode == gui::ViewMode::Details)
	{
		tab->table_model()->UpdateIndices(indices);
	} else if (view_mode == gui::ViewMode::Icons) {
		tab->icon_view()->UpdateIndices(indices);
	}
}

}
