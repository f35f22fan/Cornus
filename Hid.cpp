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
		row = tab->table()->IsOnFileNameStringNTS(pos, &file);
	} else if (view_mode == gui::ViewMode::Icons) {
		file = tab->icon_view()->GetFileAtNTS(pos, Clone::No, &row);
	}
	
	if (file != nullptr)
	{
		file->selected(!file->selected());
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
	bool on_file_name = false;
	int row = -1;
	if (view_mode == gui::ViewMode::Details)
	{
		row = tab->table()->IsOnFileNameStringNTS(pos, &file);
		on_file_name = row != -1;
		if (shift_select)
			shift_select->base_row = row;
	}
	
	if (on_file_name) {
		if (mouse_pressed) {
			if (file == nullptr || !file->selected())
				tab->SelectAllFilesNTS(false, indices);
			if (file != nullptr && !file->selected() && row != -1) {
				file->selected(true);
				indices.append(row);
			}
		}
	} else {
		tab->SelectAllFilesNTS(false, indices);
	}
}

void Hid::SelectFileByIndex(gui::Tab *tab, const int file_index,
	const gui::DeselectOthers des)
{
	io::Files &files = tab->view_files();
	QVector<int> indices;
	{
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		if (des == gui::DeselectOthers::Yes)
		{
			int i = 0;
			for (io::File *next: vec)
			{
				if (next->selected())
				{
					next->selected(false);
					indices.append(i);
				}
				i++;
			}
		}
		
		if (file_index >= 0 && file_index < vec.size())
		{
			vec[file_index]->selected(true);
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
