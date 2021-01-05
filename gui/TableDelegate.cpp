#include "TableDelegate.hpp"

#include "../io/io.hh"
#include "../io/File.hpp"
#include "Table.hpp"
#include "TableModel.hpp"

#include <mutex>
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

TableDelegate::TableDelegate(gui::Table *table):
table_(table)
{
	
}

TableDelegate::~TableDelegate() {
}

void
TableDelegate::DrawFileName(QPainter *painter, const io::File *file,
	const QStyleOptionViewItem &option, QFontMetricsF &fm,
	const QRect &text_rect) const
{
	auto &pal = option.palette;
	if (option.state & QStyle::State_Selected) {
		painter->fillRect(option.rect, pal.highlight());
		painter->setBrush(pal.highlightedText());
	} else {
		auto &brush = pal.brush(QPalette::Base);
		painter->fillRect(option.rect, brush);
		painter->setBrush(option.palette.text());
	}
	
	painter->drawText(text_rect, file->name());
	
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
	
	auto str_rect = fm.boundingRect(file->name());
	int w = text_rect.width() - str_rect.width();
	
	if (w <= 10)
		return;
	
	auto link_data_rect = QRect(text_rect.x() + str_rect.width(), text_rect.y(),
		text_rect.width() - str_rect.width(), text_rect.height());
	QBrush brush = pal.brush(QPalette::QPalette::PlaceholderText);
	QPen pen(brush.color());
	painter->setPen(pen);
	painter->drawText(link_data_rect, link_data);
	
}

void
TableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	RestorePainter rp(painter);
	QFontMetricsF fm(option.font);
	const Column col = static_cast<Column>(index.column());
	initStyleOption(const_cast<QStyleOptionViewItem*>(&option), index);
	auto *files = table_->model()->files();
	std::lock_guard<std::mutex> guard(files->mutex);
	const int row = index.row();
	if (row >= files->vec.size())
		return;
	
	io::File *file = files->vec[row];
	const int off = 3;
	auto &r = option.rect;
	QRect text_rect(r.x() + off, r.y(), r.width() - off, r.height());
	
	if (col == Column::FileName) {
		DrawFileName(painter, file, option, fm, text_rect);
	} else {
		QStyledItemDelegate::paint(painter, option, index);
	}
}

}
