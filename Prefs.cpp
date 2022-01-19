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

void Prefs::Load()
{
	const QString full_path = prefs::QueryAppConfigPath() + '/'
		+ prefs::PrefsFileName + QString::number(prefs::PrefsFormatVersion);
	io::ReadParams read_params = {};
	ByteArray buf;
	VOID_RET_IF(io::ReadFile(full_path, buf, read_params), false);
	
	if (buf.is_empty()) {
		mtl_warn("Prefs file is empty");
		return;
	}
	
	u16 version = buf.next_u16();
	VOID_RET_IF_NOT(version, prefs::PrefsFormatVersion);
	table_size_.pixels = buf.next_i16();
	table_size_.points = buf.next_i16();
	const i8 col_start = buf.next_i8();
	const i8 col_end = buf.next_i8();
	
	for (i8 i = col_start; i < col_end; i++) {
		cols_visibility_[i] = buf.next_i8() == 1 ? true : false;
	}
	
	bool_ = buf.next_u64();
	side_pane_width_ = buf.next_i32();
	editor_tab_size_ = buf.next_i8();
	splitter_sizes_.append(buf.next_i32());
	splitter_sizes_.append(buf.next_i32());
	win_w_ = buf.next_i32();
	win_h_ = buf.next_i32();
	
	
	// ABI: this line must be the last one to save unknown data to @left_bytes
	// to avoid casual abi version bumps which would entail losing prefs.
	left_bytes_ = buf.CloneFromHere();
}

void Prefs::Save() const
{
	QString parent_dir = prefs::QueryAppConfigPath();
	parent_dir.append('/');
	VOID_RET_IF(parent_dir.isEmpty(), true);
	
	auto filename = prefs::PrefsFileName + QString::number(prefs::PrefsFormatVersion);
	io::SaveFile save_file(parent_dir, filename);
	
	ByteArray buf;
	buf.add_u16(prefs::PrefsFormatVersion);
	buf.add_i16(table_size_.pixels);
	buf.add_i16(table_size_.points);
	auto *hh = app_->tab()->table()->horizontalHeader();
	const i8 col_start = (int)gui::Column::FileName + 1;
	const i8 col_end = int(gui::Column::Count);
	buf.add_i8(col_start);
	buf.add_i8(col_end);
	
	for (i8 i = col_start; i < col_end; i++)
	{
		i8 b = hh->isSectionHidden(i) ? 0 : 1;
		buf.add_i8(b);
	}
	
	buf.add_u64(bool_);
	buf.add_i32(side_pane_width_);
	buf.add_i8(editor_tab_size_);
	
	QList<int> sizes = app_->main_splitter()->sizes();
	buf.add_i32(sizes[0]);
	buf.add_i32(sizes[1]);
	const auto sz = app_->size();
	buf.add_i32(sz.width());
	buf.add_i32(sz.height());
	
	
	// ABI: this line must be the last one to add data to @buf
	buf.add(left_bytes_);
	if (io::WriteToFile(save_file.GetPathToWorkWith(), buf.data(), buf.size()) != 0)
	{
		mtl_trace("Failed to save bookmarks");
	}
	
	save_file.Commit();
}

void Prefs::UpdateTableSizes()
{
	i32 str_h = (table_size_.pixels > 0) ? table_size_.pixels : table_size_.points;
	gui::Table *table = app_->tab()->table();
	
	if (table_size_.ratio < 0) {
		QFont f = table->font();
		int rh = table->verticalHeader()->defaultSectionSize();
		int orig_sz = (f.pixelSize() > 0) ? f.pixelSize() : f.pointSize();
		table_size_.ratio = float(rh) / float(orig_sz);
	}
	
	i32 max = str_h * table_size_.ratio;
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
