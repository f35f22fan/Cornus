#include "TableDelegate.hpp"

#include "../App.hpp"
#include "../DesktopFile.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "../Media.hpp"
#include "../MutexGuard.hpp"
#include "../Prefs.hpp"
#include "RestorePainter.hpp"
#include "Tab.hpp"
#include "Table.hpp"
#include "TableModel.hpp"

#include <mutex>
#include <QDateTime>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>

namespace cornus::gui {

const Qt::Alignment AlignCenterMiddle = Qt::AlignCenter | Qt::AlignVCenter;

void ClipboardIcons::init_if_needed() {
	if (!cut.isNull())
		return;
	
	copy = QIcon::fromTheme(QLatin1String("edit-copy"));
	cut = QIcon::fromTheme(QLatin1String("edit-cut"));
	paste = QIcon::fromTheme(QLatin1String("edit-paste"));
	link = QIcon::fromTheme(QLatin1String("insert-link"));
}

TableDelegate::TableDelegate(gui::Table *table, App *app, gui::Tab *tab): app_(app),
table_(table), tab_(tab)
{
	media_ = app_->media();
	if (!media_->loaded())
		media_->Reload();
}

TableDelegate::~TableDelegate() {
}

void
TableDelegate::DrawFileName(QPainter *painter, io::File *file,
	const int row, const QStyleOptionViewItem &option,
	QFontMetrics &fm, const QRect &text_rect) const
{
	const auto filename_width = fm.horizontalAdvance(file->name());
	
	if (!file->is_dir_or_so() && file->has_exec_bit()) {
		QPen pen(app_->green_color());
		painter->setPen(pen);
	}
	
	bool paint_as_selected = false;
	if (table_->CheckIsOnFileName(file, row, table_->drop_coord())) {
		paint_as_selected = true;
	}
	
	QRectF sel_rect = option.rect;
	const int str_wide = filename_width + gui::FileNameRelax * 2;
	const int actual_wide = str_wide < min_name_w_ ? min_name_w_ : str_wide;
	sel_rect.setWidth(actual_wide);
	const bool mouse_over = table_->mouse_over_file_name_index() == row;
	
	if (mouse_over || paint_as_selected || file->is_selected() || file->selected_by_search()) {
		QColor c;
		if (file->selected_by_search()) {
			if (file->selected_by_search_active())
				c = QColor(255, 150, 150);
			else
				c = QColor(10, 255, 10);
		} else {
			c = option.palette.highlight().color();
			if (mouse_over && !file->is_selected()) {
				c = app_->hover_bg_color_gray(c);
			}
		}
		QPainterPath path;
		int less = 2;
		sel_rect.setY(sel_rect.y() + less / 2 + 1);/// +1 for line width
		sel_rect.setHeight(sel_rect.height() - less);
		path.addRoundedRect(sel_rect, 6, 6);
		painter->fillPath(path, c);
	}
	
	auto bounding_rect = text_rect;
	bounding_rect.setWidth(bounding_rect.width() + 300);
	painter->drawText(text_rect, text_alignment_, file->name(), &bounding_rect);
	
	if (file->is_regular() && file->cache().desktop_file != nullptr)
	{
		DesktopFile *df = file->cache().desktop_file;
		QString desktop_fn = df->GetName(app_->locale());
		if (!desktop_fn.isEmpty())
		{
			desktop_fn = QLatin1String(" (") + desktop_fn + ')';
			QBrush brush = option.palette.brush(QPalette::PlaceholderText);
			QPen pen(brush.color());
			painter->setPen(pen);
			auto drect = text_rect;
			drect.setX(drect.x() + bounding_rect.width());
			painter->drawText(drect, text_alignment_, desktop_fn);
		}
	} else if (file->thumbnail() != nullptr || file->has_thumbnail_attr()) {
		i32 w = -1, h = -1;
		Thumbnail *thmb = file->thumbnail();
		if (thmb != nullptr) {
			w = thmb->original_image_w;
			h = thmb->original_image_h;
		} else {
			ByteArray &ba = file->thumbnail_attrs_ref();
			thumbnail::GetOriginalImageSize(ba, w, h);
		}
		
		if (w != -1 && h != -1)
		{
			const QString img_wh_str = QChar(' ')
				+ thumbnail::SizeToString(w, h, ViewMode::Details);
			QPen saved_pen = painter->pen();
			QRect img_dim_rect = text_rect;
			img_dim_rect.setX(text_rect.x() + filename_width);
			QBrush brush = option.palette.brush(QPalette::PlaceholderText);
			QPen pen(brush.color());
			painter->setPen(pen);
			painter->drawText(img_dim_rect, text_alignment_, img_wh_str);
			painter->setPen(saved_pen);
		}
	} else if (file->is_regular() && file->has_media_attrs()) {
		DrawMediaAttrs(file, painter, option, text_rect, filename_width);
	}
	
	if (!app_->prefs().show_link_targets())
		return;
	
	if (!file->is_symlink() || (file->link_target() == nullptr))
		return;
	
	auto *t = file->link_target();
	QString how_many;
	
	if (std::abs(t->cycles) > 1) {
		how_many.append(QLatin1String(" ("));
		how_many.append(QString::number(std::abs(t->cycles)));
		how_many.append(QChar(')'));
	}
	
	QString link_data = QString(" → ");
	if (t->cycles < 0) {
		if (-t->cycles == io::LinkTarget::MaxCycles) {
			link_data.append("Symlink chain too large");
		} else {
			link_data.append("[Circular symlink!]");
		}
		link_data.append(how_many);
	} else {
		QString link_path;
		const QString &dir_path = file->dir_path(Lock::Yes);
		if (t->path.startsWith(dir_path)) {
			int size = dir_path.size();
			if (!dir_path.endsWith('/'))
				size++;
			link_path = t->path.mid(size);
		} else {
			link_path = t->path;
		}
		
		link_data.append(QChar('\"') + link_path + QChar('\"'));
		
		if (t->cycles > 1)
			link_data.append(how_many);
	}
	
	QRect link_data_rect = text_rect;
	link_data_rect.setX(text_rect.x() + filename_width);
	QBrush brush = option.palette.brush(QPalette::PlaceholderText);
	QPen pen(brush.color());
	painter->setPen(pen);
	painter->drawText(link_data_rect, text_alignment_, link_data);
}

void
TableDelegate::DrawIcon(QPainter *painter, io::File *file,
	const int row,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect) const
{
	const bool mouse_over = table_->mouse_over_file_icon_index() == row;
	
	if (mouse_over)
		painter->fillRect(option.rect, app_->hover_bg_color());
	
	QString text;
	if (file->is_symlink()) {
		if (file->link_target() != nullptr && file->link_target()->is_relative)
			text = QLatin1String("l");
		else
			text = QLatin1String("L");
	} else if (file->is_bloc_device()) {
		text = QLatin1String("B");
	} else if (file->is_char_device()) {
		text = QLatin1String("C");
	} else if (file->is_socket()) {
		text = QLatin1String("S");
	} else if (file->is_pipe()) {
		text = QLatin1String("P");
	}
	
	QBrush brush = option.palette.brush(QPalette::QPalette::PlaceholderText);
	QPen pen(brush.color());
	
	if (!app_->prefs().mark_extended_attrs_disabled() && file->has_ext_attrs()) {
		text.append("\u2022");
		QColor color;
		if (file->has_last_watched_attr()) {
			color = QColor(255, 0, 0);
		} else if (file->has_media_attrs())
			color = QColor(50, 50, 255);
		else if (file->has_thumbnail_attr()) {
			color = QColor(0, 100, 0);
		} else {
			color = QColor(100, 100, 100);
		}
		pen.setColor(color);
	}
	
	painter->setPen(pen);
	painter->drawText(text_rect, text_alignment_, text);
	
	QIcon *icon = app_->GetFileIcon(file);
	if (icon == nullptr)
		return;
	
	const bool transparent = file->action_cut() || file->action_copy() ||
		file->action_paste();
	
	if (transparent)
		painter->setOpacity(0.5f);
	
	QRect tr = fm.boundingRect(text);
	QRect icon_rect = text_rect;
	icon_rect.setX(tr.width() + 2);
	icon_rect.setWidth(icon_rect.width() - tr.width() - 2);
	icon->paint(painter, icon_rect);
	
	if (transparent) {
		painter->setOpacity(1.0f);
		clipboard_icons_.init_if_needed();
		const QIcon *action_icon = nullptr;
		
		if (file->action_copy())
			action_icon = &clipboard_icons_.copy;
		else if (file->action_cut())
			action_icon = &clipboard_icons_.cut;
		else if (file->action_paste())
			action_icon = &clipboard_icons_.paste;
		else if (file->action_paste_link())
			action_icon = &clipboard_icons_.link;
		
		if (action_icon != nullptr)
			action_icon->paint(painter, icon_rect);
	}
}

void TableDelegate::DrawMediaAttrs(io::File *file, QPainter *painter,
	const QStyleOptionViewItem &option, const QRect &text_rect,
	const int filename_w) const
{
	media::MediaPreview *m = file->media_attrs_decoded();
	mtl_check_void(m != nullptr);
	
	QString shadow_str = QLatin1String(" ");
	cbool has_year = m->year_started > 0;
	if (has_year)
	{
		shadow_str.append('(').append(QString::number(m->year_started));
		
		if (m->month_started > 0 || m->day_started > 0)
		{
			shadow_str.append('/');
			if (m->month_started > 0)
			{
				shadow_str.append(QString::number(m->month_started));
			} else {
				shadow_str.append('?');
			}
			
			shadow_str.append('/');
			if (m->day_started > 0)
			{
				shadow_str.append(QString::number(m->day_started));
			} else {
				shadow_str.append('?');
			}
		}
	}
	
	if (m->year_end > 0)
		shadow_str.append('-').append(QString::number(m->year_end));
	
	if (has_year)
		shadow_str.append(')');
	
	bool rip_added = false;
	if (!m->rips.isEmpty())
	{
		ci16 rip = m->rips[0];
		QString rs = media_->data_.rips[rip];
		if (!rs.isEmpty())
		{
			rip_added = true;
			shadow_str.append(' ').append(rs);
		}
	}
	
	if (!m->video_codecs.isEmpty())
	{
		ci16 rip = m->video_codecs[0];
		QString cs = media_->data_.video_codecs[rip];
		if (!cs.isEmpty())
		{
			if (rip_added)
				shadow_str.append('-');
			shadow_str.append(cs);
			
			if (m->bit_depth > 0)
			{
				shadow_str.append(' ').append(QString::number(m->bit_depth))
				.append(QLatin1String("bit"));
			}
		}
	}
	
	if (m->video_w > 0 && m->video_h > 0) {
		shadow_str.append(' ').append(QString::number(m->video_w)).append('x')
		.append(QString::number(m->video_h));
	}
	
	if (m->fps > 0.0) {
		shadow_str.append(' ').append(QString::number(m->fps, 'f', 2));
		shadow_str.append(QLatin1String("fps"));
	}
	
	QPen saved_pen = painter->pen();
	QRect img_dim_rect = text_rect;
	img_dim_rect.setX(text_rect.x() + filename_w);
	QBrush brush = option.palette.brush(QPalette::PlaceholderText);
	QPen pen(brush.color());
	painter->setPen(pen);
	painter->drawText(img_dim_rect, text_alignment_, shadow_str);
	painter->setPen(saved_pen);
}

void
TableDelegate::DrawSize(QPainter *painter, io::File *file,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect) const
{
	QString text = file->SizeToString();
	const auto a = file->is_dir_or_so() ? AlignCenterMiddle : text_alignment_;
	
	if (file->is_dir_or_so())
	{
		QBrush brush = option.palette.brush(QPalette::PlaceholderText);
		QPen pen(brush.color());
		painter->setPen(pen);
	}
	painter->drawText(text_rect, a, text);
}

void
TableDelegate::DrawTime(QPainter *painter, io::File *file,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect, const Column col) const
{
	const struct statx_timestamp &stx = (col == Column::TimeCreated)
		? file->time_created() : file->time_modified();
	QDateTime tm = QDateTime::fromSecsSinceEpoch(stx.tv_sec);
	static const QString format = QLatin1String("yyyy-MM-dd hh:mm");
	QString s = tm.toString(format);
	painter->drawText(text_rect, text_alignment_, s);
}

void
TableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	RestorePainter rp(painter);
	painter->setRenderHint(QPainter::Antialiasing);
	QFontMetrics fm(option.font);
	
	if (min_name_w_ == -1) {
		min_name_w_ = fm.horizontalAdvance(QLatin1String("w")) * 7;
	}
	
	const Column col = static_cast<Column>(index.column());
	initStyleOption(const_cast<QStyleOptionViewItem*>(&option), index);
	io::Files &files = tab_->view_files();
	auto g = files.guard();
	const int row = index.row();
	
	if (row >= files.data.vec.size())
		return;
	
	io::File *file = files.data.vec[row];
	auto &r = option.rect;
	QRect text_rect(r.x() + gui::FileNameRelax, r.y(),
		r.width() - gui::FileNameRelax, r.height());
	auto color_role = (row % 2) ? QPalette::AlternateBase : QPalette::Base;
	painter->fillRect(option.rect, option.palette.brush(color_role));
	
	if (option.state & QStyle::State_Selected) {
		painter->setBrush(option.palette.highlightedText());
	} else {
		painter->setBrush(option.palette.text());
	}
	
	if (col == Column::Icon) {
		DrawIcon(painter, file, row, option, fm, text_rect);
	} else if (col == Column::FileName) {
		DrawFileName(painter, file, row, option, fm, text_rect);
	} else if (col == Column::Size) {
		DrawSize(painter, file, option, fm, text_rect);
	} else if (col == Column::TimeCreated || col == Column::TimeModified) {
		DrawTime(painter, file, option, fm, text_rect, col);
	} else {
		QStyledItemDelegate::paint(painter, option, index);
	}
}

}
