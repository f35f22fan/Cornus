#include "TableDelegate.hpp"

#include "../App.hpp"
#include "../io/io.hh"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "Table.hpp"
#include "TableModel.hpp"

#include <mutex>
#include <QDateTime>
#include <QHeaderView>
#include <QPainter>

class RestorePainter {
public:
	RestorePainter(QPainter *p): p_(p) { p->save();}
	~RestorePainter() { p_->restore(); }
private:
	NO_ASSIGN_COPY_MOVE(RestorePainter);
	QPainter *p_ = nullptr;
};

namespace cornus::gui {
const int FnOff = 2;

TableDelegate::TableDelegate(gui::Table *table, App *app): app_(app),
table_(table)
{
	
}

TableDelegate::~TableDelegate() {
}

void
TableDelegate::DrawFileName(QPainter *painter, io::File *file,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect) const
{
	const int rh = table_->verticalHeader()->defaultSectionSize();
	auto str_rect = fm.boundingRect(file->name());
	
	if (!file->is_dir_or_so() && file->has_exec_bit()) {
		QPen pen(QColor(0, 100, 0));
		painter->setPen(pen);
	}
	
	if (file->selected()) {
		QRect r = option.rect;
		r.setWidth(str_rect.width() + FnOff * 2);
		painter->fillRect(r, option.palette.highlight());
	}
	
	const int str_h = str_rect.height();
	int text_y = text_rect.y() + str_h - (rh - str_h) / 2;
	painter->drawText(text_rect.x(), text_y, file->name());
	
	if (!file->is_symlink() || (file->link_target() == nullptr))
		return;
	
	auto *t = file->link_target();
	QString how_many;
	
	if (std::abs(t->cycles) > 1) {
		how_many.append(QLatin1String(" ("));
		how_many.append(QString::number(std::abs(t->cycles)));
		how_many.append(QChar(')'));
	}
	
	QString link_data = QString("  🠚 ");
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
	
	int w = text_rect.width() - str_rect.width();
	
	if (w <= 10)
		return;
	
	auto link_data_rect = QRect(text_rect.x() + str_rect.width(), text_rect.y(),
		text_rect.width() - str_rect.width(), text_rect.height());
	QBrush brush = option.palette.brush(QPalette::QPalette::PlaceholderText);
	QPen pen(brush.color());
	painter->setPen(pen);
	painter->drawText(link_data_rect, link_data);
	
}

void
TableDelegate::DrawIcon(QPainter *painter, io::File *file,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect) const
{
	QString text;
	if (file->is_regular()) {
	} else if (file->is_symlink()) {
		text = QLatin1String("L");
	} else if (file->is_bloc_device()) {
		text = QLatin1String("B");
	} if (file->is_char_device()) {
		text = QLatin1String("C");
	} if (file->is_socket()) {
		text = QLatin1String("S");
	} if (file->is_pipe()) {
		text = QLatin1String("P");
	}
	
	QBrush brush = option.palette.brush(QPalette::QPalette::PlaceholderText);
	QPen pen(brush.color());
	painter->setPen(pen);
	painter->drawText(text_rect, text);
	
	QIcon *icon = nullptr;
	if (file->cache().icon != nullptr)
		icon = file->cache().icon;
	
	if (icon == nullptr) {
		auto *app = table_->model()->app();
		app->LoadIcon(*file);
		icon = file->cache().icon;
	}
	
	if (icon != nullptr) {
		QRect tr = fm.boundingRect(text);
		QRect icon_rect = text_rect;
		icon_rect.setX(tr.width() + 2);
		icon_rect.setWidth(icon_rect.width() - tr.width() - 2);
		icon->paint(painter, icon_rect);
	}
}

void
TableDelegate::DrawSize(QPainter *painter, io::File *file,
	const QStyleOptionViewItem &option, QFontMetrics &fm,
	const QRect &text_rect) const
{
	QString text = file->SizeToString();
	painter->drawText(text_rect, text);
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
	painter->drawText(text_rect, s);
}

void
TableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	RestorePainter rp(painter);
	QFontMetrics fm(option.font);
	const Column col = static_cast<Column>(index.column());
	initStyleOption(const_cast<QStyleOptionViewItem*>(&option), index);
	io::Files &files = app_->view_files();
	MutexGuard guard(&files.mutex);
	const int row = index.row();
	if (row >= files.data.vec.size())
		return;
	
	io::File *file = files.data.vec[row];
	auto &r = option.rect;
	QRect text_rect(r.x() + FnOff, r.y(), r.width() - FnOff, r.height());
	auto color_role = (row % 2) ? QPalette::AlternateBase : QPalette::Base;
	painter->fillRect(option.rect, option.palette.brush(color_role));
	
	if (option.state & QStyle::State_Selected) {
		//painter->fillRect(option.rect, option.palette.highlight());
		painter->setBrush(option.palette.highlightedText());
	} else {
		//painter->fillRect(option.rect, option.palette.brush(color_role));
		painter->setBrush(option.palette.text());
	}
	
	if (col == Column::Icon) {
		DrawIcon(painter, file, option, fm, text_rect);
	} else if (col == Column::FileName) {
		DrawFileName(painter, file, option, fm, text_rect);
	} else if (col == Column::Size) {
		DrawSize(painter, file, option, fm, text_rect);
	} else if (col == Column::TimeCreated || col == Column::TimeModified) {
		DrawTime(painter, file, option, fm, text_rect, col);
	} else {
		QStyledItemDelegate::paint(painter, option, index);
	}
}

}
