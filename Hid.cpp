#include "Hid.hpp"

#include "App.hpp"
#include "gui/IconView.hpp"
#include "gui/Tab.hpp"
#include "gui/Table.hpp"
#include "gui/TableModel.hpp"
#include "io/File.hpp"
#include "io/Files.hpp"

namespace cornus {

Hid::Hid(App *app) : app_(app)
{
	Q_UNUSED(app_);
}

Hid::~Hid()
{}

void Hid::HandleKeySelect(gui::Tab *tab, const VDirection vdir,
	const int key, const Qt::KeyboardModifiers modifiers)
{
	if (modifiers & Qt::ControlModifier)
		return;
	
	io::Files &files = tab->view_files();
	const int file_count = files.cached_files_count;
	gui::ShiftSelect *shift_select = tab->ShiftSelect();
	MTL_CHECK_VOID(shift_select != nullptr);
	
	int old_file_index = files.GetFirstSelectedFile(Lock::Yes);
	if (old_file_index == -1)
		old_file_index = shift_select->base_row;
	
	cbool was_set_to_zero = (old_file_index == -1);
	if (was_set_to_zero)
		old_file_index = 0;
	
	if (vdir == VDirection::Up && (!was_set_to_zero && old_file_index == 0))
		return;
	
	if (vdir == VDirection::Down && old_file_index >= file_count - 1)
		return;
	
	cbool is_icon_view = tab->view_mode() == gui::ViewMode::Icons;
	cbool up_or_down = key == Qt::Key_Up || key == Qt::Key_Down;
	int new_file_index = -1;
	QSet<int> indices;
	{
		MutexGuard guard = files.guard();
		files.SelectAllFiles(Lock::No, Selected::No, indices);
		if (was_set_to_zero)
		{
			new_file_index = (vdir == VDirection::Up) ? 0 : file_count - 1;
			if (new_file_index >= 0)
			{
				indices.insert(new_file_index);
				files.data.vec[new_file_index]->set_selected(true);
			}
		} else {
			if (is_icon_view && up_or_down)
			{
				const int n = was_set_to_zero ? -1 : old_file_index;
				new_file_index = tab->icon_view()->CellIndexInNextRow(n, vdir);
			} else {
				new_file_index = old_file_index + ((vdir == VDirection::Up) ? -1 : 1);
			}
			
			if (new_file_index >= 0 && new_file_index < file_count)
			{
				io::File *file = files.data.vec[old_file_index];
				file->set_selected(false);
				files.data.vec[new_file_index]->set_selected(true);
				indices.insert(new_file_index);
				indices.insert(old_file_index);
			}
		}
	}
	
	if (new_file_index != -1) {
		tab->ScrollToFile(new_file_index);
	}
	
	tab->UpdateIndices(indices);
}

void Hid::HandleKeyShiftSelect(gui::Tab *tab, const VDirection vdir,
	const int key)
{
	auto &files = tab->view_files();
	int row = files.GetFirstSelectedFile(Lock::Yes);
	if (row == -1)
		return;
	
	QSet<int> indices;
	gui::ShiftSelect *shift_select = tab->ShiftSelect();
	if (shift_select->head_row == -1)
		shift_select->head_row = shift_select->base_row;
	
	if (shift_select->base_row == -1) {
		shift_select->base_row = row;
		shift_select->head_row = row;
		MutexGuard guard = files.guard();
		files.data.vec[row]->set_selected(true);
		indices.insert(row);
	} else {
		if (vdir == VDirection::Up) {
			if (shift_select->head_row == 0)
				return;
			shift_select->head_row--;
		} else {
			const int count = tab->table_model()->rowCount();
			if (shift_select->head_row == count -1)
				return;
			shift_select->head_row++;
		}
		{
			MutexGuard guard = files.guard();
			files.SelectAllFiles(Lock::No, Selected::No, indices);
			files.SelectFileRange(Lock::No, shift_select->base_row,
				shift_select->head_row, indices);
		}
	}
	
	if (shift_select->head_row != -1) {
		tab->ScrollToFile(shift_select->head_row);
	}
	
	tab->UpdateIndices(indices);
}

void Hid::HandleMouseSelectionShift(gui::Tab *tab, const QPoint &pos,
	QSet<int> &indices)
{
	io::Files &files = tab->view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	int file_index = -1;
	gui::ShiftSelect *shift_select = nullptr;
	if (tab->view_mode() == gui::ViewMode::Details)
	{
		file_index = tab->table()->GetFileAt_NoLock(pos, PickedBy::VisibleName, &file);
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
			indices.insert(file_index);
		} else {
			files.SelectAllFiles(Lock::No, Selected::No, indices);
			files.SelectFileRange(Lock::No, shift_select->base_row, file_index, indices);
		}
	}
}

void Hid::HandleMouseSelectionCtrl(gui::Tab *tab, const QPoint &pos,
	QSet<int> *indices)
{
	const gui::ViewMode view_mode = tab->view_mode();
	io::Files &files = tab->view_files();
	MutexGuard guard = files.guard();
	
	io::File *file = nullptr;
	int row = -1;
	if (view_mode == gui::ViewMode::Details)
	{
		row = tab->table()->GetFileAt_NoLock(pos, PickedBy::VisibleName, &file);
	} else if (view_mode == gui::ViewMode::Icons) {
		file = tab->icon_view()->GetFileAt_NoLock(pos, Clone::No, &row);
	}
	
	if (file != nullptr)
	{
		file->toggle_selected();
		if (indices != nullptr && row != -1)
			indices->insert(row);
	}
}

void Hid::HandleMouseSelectionNoModif(gui::Tab *tab, const QPoint &pos, QSet<int> &indices,
	bool mouse_pressed, gui::ShiftSelect *shift_select)
{
	const gui::ViewMode view_mode = tab->view_mode();
	io::Files &files = tab->view_files();
	auto g = files.guard();
	
	io::File *file = nullptr;
	int file_index = -1;
	if (view_mode == gui::ViewMode::Details)
	{
		file_index = tab->table()->GetFileAt_NoLock(pos, PickedBy::VisibleName, &file);
		if (file_index == -1) {
			file_index = tab->table()->GetFileAt_NoLock(pos, PickedBy::Icon, &file);
		}
	} else if (view_mode == gui::ViewMode::Icons) {
		file = tab->icon_view()->GetFileAt_NoLock(pos, Clone::No, &file_index);
	}
	
	if (shift_select)
		shift_select->base_row = file_index;
	
	if (file_index != -1)
	{
		if (mouse_pressed)
		{
			if (file == nullptr || !file->is_selected()) {
				files.SelectAllFiles(Lock::No, Selected::No, indices);
			}
			if (file != nullptr && !file->is_selected() && file_index != -1) {
				file->selected(Selected::Yes);
				indices.insert(file_index);
			}
		}
	} else {
		files.SelectAllFiles(Lock::No, Selected::No, indices);
	}
}

void Hid::SelectFileByIndex(gui::Tab *tab, const int file_index,
	const DeselectOthers des)
{
	io::Files &files = tab->view_files();
	QSet<int> indices;
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		if (des == DeselectOthers::Yes)
		{
			int i = -1;
			for (io::File *next: vec)
			{
				i++;
				if (i != file_index && next->is_selected())
				{
					next->toggle_selected();
					indices.insert(i);
				}
			}
		}
		
		if (file_index >= 0 && file_index < vec.size())
		{
			vec[file_index]->selected(Selected::Yes);
			indices.insert(file_index);
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
