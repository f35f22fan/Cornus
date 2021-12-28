#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/io.hh"
#include "../ElapsedTimer.hpp"

QT_BEGIN_NAMESPACE
class QScrollBar;
QT_END_NAMESPACE

#include <QWidget>

#include <chrono>
using namespace std::chrono;

namespace cornus::gui {

enum class DrawBorder: i8 {
	Yes,
	No
};

struct IconDim {
	double total_h = 0;
	double w = 0;
	double w_and_gaps = 0;
	double cell_and_gap = 0;
	double h = 0;
	double h_and_gaps = 0;
	double rh = 0;
	double gap = 0;
	double two_gaps = 0;
	int row_count = -1;
	int per_row = -1;
	int text_y = -1;
	int str_h = -1;
	int text_h = -1;
	int text_rows = -1;
};

struct Last {
	int file_count = -1;
	int row_count = -1;
};

class IconView : public QWidget {
public:
	IconView(App *app, Tab *tab, QScrollBar *vs);
	virtual ~IconView();
	
	QSize minimumSize() const { return size(); }
	QSize maximumSize() const { return size(); }
	QSize size() const;
	virtual QSize sizeHint() const override { return size(); }
	
	void FilesChanged(const Repaint r, const int file_index);
	
	void RepaintLater(const int file_index);
	
protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void wheelEvent(QWheelEvent *evt) override;
	virtual void paintEvent(QPaintEvent *ev) override;
	virtual void resizeEvent(QResizeEvent *ev) override;

private:
	NO_ASSIGN_COPY_MOVE(IconView);
	
	void ComputeProportions(IconDim &dim) const;
	void DelayedRepaint();
	DrawBorder DrawThumbnail(io::File *file, QPainter &painter,
		double x, double y);
	void UpdateScrollRange();
	
	App *app_ = nullptr;
	Tab *tab_ = nullptr;
	float zoom_ = 1.0f;
	Last last_ = {};
	mutable IconDim icon_dim_ = {};
	QScrollBar *vs_ = nullptr;
	bool pending_update_ = false;
	QVector<int> repaint_indices_;
	ElapsedTimer last_repaint_;
	const i64 delay_repaint_ms_ = 500;
};



}
