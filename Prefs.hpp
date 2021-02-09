#pragma once

#include "decl.hxx"
#include "prefs.hh"

#include <QMap>

namespace cornus {

namespace prefs {
	const u64 ShowHiddenFiles = 1u << 0;
	const u64 ShowMsFilesLoaded = 1u << 1;
	const u64 ShowFreePartitionSpace = 1u << 2;
	const u64 ShowLinkTargets = 1u << 3;
}

class Prefs {
public:
	Prefs(App *app);
	virtual ~Prefs();
	
	void Load();
	void Save();
	
	inline void toggle_bool(const bool b, const u64 flag) {
		if (b)
			bool_ |= flag;
		else
			bool_ &= ~flag;
	}
	
	bool show_hidden_files() const { return bool_ & prefs::ShowHiddenFiles; }
	void show_hidden_files(bool b) { toggle_bool(b, prefs::ShowHiddenFiles); }
	
	bool show_ms_files_loaded() const { return bool_ & prefs::ShowMsFilesLoaded; }
	void show_ms_files_loaded(bool b) { toggle_bool(b, prefs::ShowMsFilesLoaded); }
	
	bool show_free_partition_space() const { return bool_ & prefs::ShowFreePartitionSpace; }
	void show_free_partition_space(bool b) { toggle_bool(b, prefs::ShowFreePartitionSpace); }
	
	bool show_link_targets() const { return bool_ & prefs::ShowLinkTargets; }
	void show_link_targets(bool b) { toggle_bool(b, prefs::ShowLinkTargets); }
	
	const QList<int>& splitter_sizes() const { return splitter_sizes_; }
	QMap<i8, bool>& cols_visibility() { return cols_visibility_; }
private:
	u64 bool_ = 0;

	i8 editor_tab_size_ = 4;
/// -1 means not explicitly set, -2 hidden:
	i32 side_pane_width_ = -1;
	QMap<i8, bool> cols_visibility_;
	QList<int> splitter_sizes_;
	
	App *app_ = nullptr;
};

}
