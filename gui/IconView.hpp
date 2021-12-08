#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/io.hh"

#include <QFontMetrics>
#include <QWidget>

namespace cornus::gui {
// view-list-icons, view-list-details
class IconView : public QWidget {
public:
	IconView(App *app, Table *table);
	virtual ~IconView();
	
	io::Files& view_files();

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
};



}
