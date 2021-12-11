#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/io.hh"

QT_FORWARD_DECLARE_CLASS(QScrollArea)
#include <QWidget>

namespace cornus::gui {

struct IconDim {
	int per_row = -1;
	int size = -1;
	int gap = -1;
	int max_w = -1;
	int max_icon_h = -1;
	int in_between = -1;
	int row_count = -1;
};

struct Last {
	int file_count = -1;
	int row_count = -1;
};

class IconView : public QWidget {
public:
	IconView(App *app, Table *table, QScrollArea *sa);
	virtual ~IconView();
	
	io::Files& view_files();
	
	QSize minimumSize() const;
	QSize maximumSize() const;
	virtual QSize sizeHint() const override;
	
protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void wheelEvent(QWheelEvent *evt) override;
	virtual void paintEvent(QPaintEvent *ev) override;

private:
	NO_ASSIGN_COPY_MOVE(IconView);
	
	App *app_ = nullptr;
	Table *table_ = nullptr;
	float zoom_ = 1.0f;
	Last last_ = {};
	IconDim icon_dim_ = {};
	QScrollArea *scroll_area_ = nullptr;
};



}
