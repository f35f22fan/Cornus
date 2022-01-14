#include "TableDelegate.hpp"

#include "../App.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
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
}

TableDelegate::~TableDelegate() {
}

void
TableDelegate::DrawFileName(QPainter *painter, io::File *file,
	const int row, const QStyleOptionViewItem &option,
	QFontMetrics &fm, const QRect &text_rect) const
{
	auto str_rect = fm.boundingRect(file->name());
	
	if (!file->is_dir_or_so() && file->has_exec_bit()) {
		QPen pen(app_->green_color());
		painter->setPen(pen);
	}
	
	bool paint_as_selected = false;
	if (table_->CheckIsOnFileName(file, row, table_->drop_coord())) {
		paint_as_selected = true;
	}
	
	QRectF sel_rect = option.rect;
	const int str_wide = str_rect.width() + gui::FileNameRelax * 2;
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
	
	auto rcopy = text_rect;
	rcopy.setWidth(rcopy.width() + 300);
	painter->drawText(text_rect, text_alignment_, file->name(), &rcopy);
	
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
		const QString &dir_path = file->dir_path();
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
	link_data_rect.setX(text_rect.x() + str_rect.width());
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
		if (file->has_media_attrs())
			pen.setColor(QColor(50, 50, 255));
		else if (file->has_thumbnail_attr())
			pen.setColor(QColor(0, 100, 0));
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
		min_name_w_ = fm.boundingRect(QLatin1String("w")).width() * 7;
	}
	
	const Column col = static_cast<Column>(index.column());
	initStyleOption(const_cast<QStyleOptionViewItem*>(&option), index);
	io::Files &files = *app_->files(tab_->files_id());
	MutexGuard guard = files.guard();
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
