#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "../io/decl.hxx"

#include <QFontMetrics>
#include <QStyledItemDelegate>

namespace cornus::gui {

struct ClipboardIcons {
	QIcon cut;
	QIcon copy;
	QIcon paste;
	QIcon link;
	
	void init_if_needed ();
};

class TableDelegate: public QStyledItemDelegate {
public:
	TableDelegate(gui::Table *table, App *app, Tab *tab);
	virtual ~TableDelegate();
	
	int min_name_w() const { return min_name_w_; }
	
	virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const;
	
private:
	void DrawFileName(QPainter *painter, io::File *file, const int row,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawIcon(QPainter *painter, io::File *file, const int row,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawMediaAttrs(io::File *file, QPainter *painter,
		const QStyleOptionViewItem &option, const QRect &text_rect,
		const int filename_w) const;
	
	void DrawSize(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect) const;
	
	void DrawTime(QPainter *painter, io::File *file,
		const QStyleOptionViewItem &option, QFontMetrics &fm,
		const QRect &text_rect, const Column col) const;
	
	App *app_ = nullptr;
	gui::Table *table_ = nullptr;
	gui::Tab *tab_ = nullptr;
	Media *media_ = nullptr;
	Qt::Alignment text_alignment_ = Qt::AlignLeft | Qt::AlignVCenter;
	
	mutable int min_name_w_ = -1;
	mutable ClipboardIcons clipboard_icons_ = {};
};

}
