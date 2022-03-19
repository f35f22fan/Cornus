#include "Prefs.hpp"

#include "App.hpp"
#include "ByteArray.hpp"
#include "io/io.hh"
#include "io/SaveFile.hpp"
#include "gui/Tab.hpp"
#include "gui/Table.hpp"
#include "gui/TreeView.hpp"

#include <QApplication>
#include <QHeaderView>

#include <fcntl.h>

namespace cornus {

int NormalFontSize()
{
	const QFont f = QApplication::font();
	return (f.pixelSize() > 0) ? f.pixelSize() : f.pointSize();
}

Prefs::Prefs(App *app): app_(app) {}

Prefs::~Prefs()
{
	delete left_bytes_;
	left_bytes_ = nullptr;
}

void Prefs::ApplyTableHeight(gui::Table *table, int max)
{
	if (table_size_.empty())
		return;

	auto *vh = table->verticalHeader();
	QFont f = table->font();
	if (table_size_.pixels > 0) {
		f.setPixelSize(table_size_.pixels);
	} else {
		f.setPointSize(table_size_.points);
	}
	table->setFont(f);
	vh->setSectionResizeMode(QHeaderView::Fixed);
	vh->setMinimumSectionSize((table_size_.pixels > 0)
		? table_size_.pixels : table_size_.points);
	vh->setMaximumSectionSize(max);
	vh->setDefaultSectionSize(max);
	table->UpdateColumnSizes();
}

void Prefs::ApplyTreeViewHeight()
{
	if (table_size_.empty())
		return;

	gui::TreeView *view = app_->tree_view();
	QFont f = view->font();
	if (table_size_.pixels > 0) {
		f.setPixelSize(table_size_.pixels);
	} else {
		f.setPointSize(table_size_.points);
	}
	view->setFont(f);
}

bool Prefs::Load()
{
	const QString full_path = prefs::QueryAppConfigPath() + '/'
		+ prefs::PrefsFileName + QString::number(prefs::PrefsFormatVersion);
	io::ReadParams read_params = {};
	read_params.can_rely = CanRelyOnStatxSize::Yes;
	read_params.print_errors = PrintErrors::Yes;
	ByteArray buf;
	MTL_CHECK(io::ReadFile(full_path, buf, read_params));
	MTL_CHECK(!buf.is_empty());
	
	u2 version = buf.next_u2();
	MTL_CHECK(version == prefs::PrefsFormatVersion);
	table_size_.pixels = buf.next_i2();
	table_size_.points = buf.next_i2();
	const i1 col_start = buf.next_i1();
	const i1 col_end = buf.next_i1();
	
	for (i1 i = col_start; i < col_end; i++) {
		cols_visibility_[i] = buf.next_i1() == 1 ? true : false;
	}
	
	bool_ = buf.next_u8();
	side_pane_width_ = buf.next_i4();
	editor_tab_size_ = buf.next_i1();
	splitter_sizes_.append(buf.next_i4());
	splitter_sizes_.append(buf.next_i4());
	win_w_ = buf.next_i4();
	win_h_ = buf.next_i4();
	
	// ABI: this line must be the last one to save unknown data to @left_bytes
	// to avoid casual abi version bumps which would entail losing prefs.
	left_bytes_ = buf.CloneFromHere();
	
	return true;
}

void Prefs::Save() const
{
	QString parent_dir = prefs::QueryAppConfigPath();
	parent_dir.append('/');
	MTL_CHECK_VOID(!parent_dir.isEmpty());
	
	auto filename = prefs::PrefsFileName + QString::number(prefs::PrefsFormatVersion);
	io::SaveFile save_file(parent_dir, filename);
	
	ByteArray buf;
	buf.add_u2(prefs::PrefsFormatVersion);
	buf.add_i2(table_size_.pixels);
	buf.add_i2(table_size_.points);
	auto *hh = app_->tab()->table()->horizontalHeader();
	const i1 col_start = (int)gui::Column::FileName + 1;
	const i1 col_end = int(gui::Column::Count);
	buf.add_i1(col_start);
	buf.add_i1(col_end);
	
	for (i1 i = col_start; i < col_end; i++)
	{
		i1 b = hh->isSectionHidden(i) ? 0 : 1;
		buf.add_i1(b);
	}
	
	buf.add_u8(bool_);
	buf.add_i4(side_pane_width_);
	buf.add_i1(editor_tab_size_);
	
	QList<int> sizes = app_->main_splitter()->sizes();
	buf.add_i4(sizes[0]);
	buf.add_i4(sizes[1]);
	const auto sz = app_->size();
	buf.add_i4(sz.width());
	buf.add_i4(sz.height());
	
	// ABI: this line must be the last one to add data to @buf
	buf.add(left_bytes_, From::Start);
	if (!io::WriteToFile(save_file.GetPathToWorkWith(), buf.data(), buf.size()))
	{
		mtl_trace("Failed to save bookmarks");
	}
	
	save_file.Commit();
}

void Prefs::UpdateTableSizes()
{
	i4 str_h = (table_size_.pixels > 0) ? table_size_.pixels : table_size_.points;
	gui::Table *table = app_->tab()->table();
	
	if (table_size_.ratio < 0) {
		QFont f = table->font();
		int rh = table->verticalHeader()->defaultSectionSize();
		int orig_sz = (f.pixelSize() > 0) ? f.pixelSize() : f.pointSize();
		table_size_.ratio = float(rh) / float(orig_sz);
	}
	
	i4 max = str_h * table_size_.ratio;
	ApplyTableHeight(table, max);
	ApplyTreeViewHeight();
}

void Prefs::WheelEventFromMainView(const Zoom zoom)
{
	gui::Table *table = app_->tab()->table();
	if (table_size_.empty()) {
		QFont f = table->font();
		if (f.pixelSize() > 0) {
			table_size_.pixels = f.pixelSize();
		} else {
			table_size_.points = f.pointSize();
		}
	}
	
	auto &value = (table_size_.pixels > 0) ? table_size_.pixels : table_size_.points;
	if (zoom == Zoom::In) {
		if (value < 50)
			value++;
	} else if (zoom == Zoom::Out) {
		if (value > 8)
			value--;
	} else if (zoom == Zoom::Reset) {
		value = NormalFontSize();
	}
	
	UpdateTableSizes();
}
}
