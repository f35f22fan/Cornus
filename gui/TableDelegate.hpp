#pragma once

#include "decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"

#include <QFontMetrics>
#include <QStyledItemDelegate>

Q_DECLARE_METATYPE(cornus::io::File*);

namespace cornus::gui {

class TableDelegate: public QStyledItemDelegate {
public:
	TableDelegate(gui::Table *table);
	virtual ~TableDelegate();
	
	virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const;
	
private:
	void DrawFileName(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawIcon(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawSize(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawTime(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect, const Column col) const;
	
	gui::Table *table_ = nullptr;
};

}
