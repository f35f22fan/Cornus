#include "Prefs.hpp"

#include "App.hpp"
#include "io/io.hh"
#include "gui/Table.hpp"

#include <QHeaderView>

#include <fcntl.h>

namespace cornus {

Prefs::Prefs(App *app): app_(app) {}

Prefs::~Prefs() {}

void Prefs::Load()
{
	const QString full_path = prefs::QueryAppConfigPath() + '/'
		+ prefs::PrefsFileName + QString::number(prefs::PrefsFormatVersion);
	
	ByteArray buf;
	if (io::ReadFile(full_path, buf) != io::Err::Ok)
		return;
	
	u16 version = buf.next_u16();
	CHECK_TRUE_VOID((version == prefs::PrefsFormatVersion));
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
}

void Prefs::Save() const
{
	QString parent_dir = prefs::QueryAppConfigPath();
	parent_dir.append('/');
	CHECK_TRUE_VOID(!parent_dir.isEmpty());
	const QString full_path = parent_dir + prefs::PrefsFileName 
		+ QString::number(prefs::PrefsFormatVersion);
	const QByteArray path_ba = full_path.toLocal8Bit();
	
	if (!io::FileExists(full_path)) {
		int fd = open(path_ba.data(), O_RDWR | O_CREAT | O_EXCL, 0664);
		if (fd == -1) {
			if (errno == EEXIST) {
				mtl_warn("File already exists");
			} else {
				mtl_warn("Can't create file at: \"%s\", reason: %s",
					path_ba.data(), strerror(errno));
			}
			return;
		} else {
			::close(fd);
		}
	}
	
	ByteArray buf;
	buf.add_u16(prefs::PrefsFormatVersion);
	auto *hh = app_->table()->horizontalHeader();
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
	
	if (io::WriteToFile(full_path, buf.data(), buf.size()) != io::Err::Ok) {
		mtl_trace("Failed to save bookmarks");
	}
}

}


