#pragma once

#include "decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"

#include <QFontMetricsF>
#include <QStyledItemDelegate>

Q_DECLARE_METATYPE(cornus::io::File*);

namespace cornus::gui {

class TableDelegate: public QStyledItemDelegate {
public:
	TableDelegate(gui::Table *table);
	virtual ~TableDelegate();
	
	virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;
	
private:
	void DrawFileName(QPainter *painter, const io::File *file,
		const QStyleOptionViewItem &option, QFontMetricsF &fm,
		const QRect &text_rect) const;
	
	gui::Table *table_ = nullptr;
};

}
