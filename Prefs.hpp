#pragma once

#include "decl.hxx"
#include "prefs.hh"
#include "gui/BasicTable.hpp"

#include <QMap>
#include <QTableView>

namespace cornus {

struct TableSize {
	i16 pixels = -1;
	i16 points = -1;
	float ratio = -1;
	bool empty() const { return pixels == -1 && points == -1; }
};

class Prefs {
	const u64 ShowHiddenFiles = 1u << 0;
	const u64 ShowMsFilesLoaded = 1u << 1;
	const u64 ShowFreePartitionSpace = 1u << 2;
	const u64 ShowLinkTargets = 1u << 3;
	const u64 MarkExtendedAttrsDisabled = 1u << 4;
	const u64 RememberWindowSize = 1u << 5;
public:
	Prefs(App *app);
	virtual ~Prefs();
	
	Prefs Defaults() const { return Prefs(app_); }
	
	void Load();
	void Save() const;
	
	inline void toggle_bool(const bool b, const u64 flag) {
		if (b)
			bool_ |= flag;
		else
			bool_ &= ~flag;
	}
	
	bool mark_extended_attrs_disabled() const { return bool_ & MarkExtendedAttrsDisabled; }
	void mark_extended_attrs_disabled(bool b) { toggle_bool(b, MarkExtendedAttrsDisabled); }
	
	bool remember_window_size() const { return bool_ & RememberWindowSize; }
	void remember_window_size(bool b) { toggle_bool(b, RememberWindowSize); }
	
	bool show_hidden_files() const { return bool_ & ShowHiddenFiles; }
	void show_hidden_files(bool b) { toggle_bool(b, ShowHiddenFiles); }
	
	bool show_ms_files_loaded() const { return bool_ & ShowMsFilesLoaded; }
	void show_ms_files_loaded(bool b) { toggle_bool(b, ShowMsFilesLoaded); }
	
	bool show_free_partition_space() const { return bool_ & ShowFreePartitionSpace; }
	void show_free_partition_space(bool b) { toggle_bool(b, ShowFreePartitionSpace); }
	
	bool show_link_targets() const { return bool_ & ShowLinkTargets; }
	void show_link_targets(bool b) { toggle_bool(b, ShowLinkTargets); }
	
	const QList<int>& splitter_sizes() const { return splitter_sizes_; }
	QMap<i8, bool>& cols_visibility() { return cols_visibility_; }
	i16 custom_table_font_size() const {
		return (table_size_.pixels > 0) ? table_size_.pixels : table_size_.points;
	}
	void AdjustCustomTableSize(const Zoom zoom);
	void UpdateTableSizes();
	QSize window_size() const { return QSize(win_w_, win_h_); }
	
private:
	void ApplyTableHeight(cornus::gui::BasicTable *table, int max);
	
	
	u64 bool_ = ShowLinkTargets | RememberWindowSize;
	TableSize table_size_ = {};
	/// -1 means not explicitly set, -2 hidden:
	i32 side_pane_width_ = -1;
	i8 editor_tab_size_ = 4;
	QMap<i8, bool> cols_visibility_;
	QList<int> splitter_sizes_;
	i32 win_w_ = -1, win_h_ = -1;
	
	App *app_ = nullptr;
};

}
