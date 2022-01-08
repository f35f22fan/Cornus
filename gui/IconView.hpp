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
	
	void DisplayingNewDirectory(const DirId dir_id);
	
	int GetRowAtY(const int y, int *y_off = nullptr) const
	{
		int row = y / (int)icon_dim_.rh;
		const int int_off = y % (int)icon_dim_.rh;
		if (y_off)
			*y_off = -int_off;
		
		if (int_off != 0)
			row++;
		
		return row;
	}
	
	QSize minimumSize() const { return size(); }
	QSize maximumSize() const { return size(); }
	void SendLoadingNewThumbnailsBatch();
	QSize size() const;
	virtual QSize sizeHint() const override { return size(); }
	
	void Scroll(const VDirection d, const ScrollBy sb);
	void ScrollToAndSelect(const int file_index, const DeselectOthers des);
	void ScrollToFile(const int file_index);
	void FilesChanged(const Repaint r, const int file_index);
	bool is_current_view() const;
	io::File* GetFileAtNTS(const QPoint &pos, const Clone c, int *ret_index = nullptr);
	void RepaintLater(const int custom_ms = -1);
	void UpdateIndices(const QVector<int> &indices);
	
protected:
	virtual void keyPressEvent(QKeyEvent *evt) override;
	virtual void keyReleaseEvent(QKeyEvent *evt) override;
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
	DrawBorder DrawThumbnail(io::File *file, QPainter &painter, double x, double y);
	void Init();
	void UpdateScrollRange();
	
	App *app_ = nullptr;
	Tab *tab_ = nullptr;
	float zoom_ = 1.0f;
	Last last_ = {};
	mutable IconDim icon_dim_ = {};
	QScrollBar *vs_ = nullptr;
	QVector<int> repaint_indices_;
	ElapsedTimer last_repaint_;
	bool delayed_repaint_pending_ = false;
	const i64 delay_repaint_ms_ = 100;
	bool repaint_without_delay_ = false;
	int scroll_page_step_ = -1;
	DirId last_submitted_dir_id_ = -1;
};



}
