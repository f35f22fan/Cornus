#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/io.hh"
#include "../ElapsedTimer.hpp"

#include <cmath>

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
	double text_y = -1;
	double line_h = -1;
	double text_h = -1;
	int per_row = 1;
	int text_rows = -1;
	int row_count = -1;
};

struct Last {
	int file_count = -1;
	int row_count = -1;
};

class IconView : public QWidget {
public:
	IconView(App *app, Tab *tab, QScrollBar *vs);
	virtual ~IconView();
	
	void DisplayingNewDirectory(const DirId dir_id, const Reload r);
	i32 GetFileIndexAt(const QPoint &pos) const;
	int GetRowAtY(const int y) const
	{
		return int(double(y) / icon_dim_.rh);
	}
	int GetRowHeight() const { return icon_dim_.rh; }
	i32 GetVisibleFileIndex();
	void HiliteFileUnderMouse();
	bool magnified() const { return magnified_; }
	void magnified(const bool b) { magnified_ = b; }
	QSize minimumSize() const { return size(); }
	QSize maximumSize() const { return size(); }
	int CellIndexInNextRow(const int file_index, const VDirection vdir);
	void SendLoadingNewThumbnailsBatch();
	void SendLoadingNewThumbnail(io::File *cloned_file);
	void SetViewState(const NewState ns);
	QSize size() const;
	virtual QSize sizeHint() const override { return size(); }
	
	void ScrollByWheel(const VDirection d, const ScrollBy sb);
	void ScrollToAndSelect(const int file_index, const DeselectOthers des);
	void ScrollToFile(const int file_index);
	void FileChanged(const io::FileEventType evt, io::File *cloned_file = nullptr);
	bool is_current_view() const;
	io::File* GetFileAt_NoLock(const QPoint &pos, const Clone c, int *ret_index = nullptr);
	void RepaintLater(const int custom_ms = -1);
	ShiftSelect* shift_select() { return &shift_select_; }
	void UpdateIndex(const int file_index);
	void UpdateFileIndexRange(const int start, const int end);
	void UpdateIndices(const QSet<int> &indices);
	void UpdateVisibleArea() { update(); }
	QScrollBar* scrollbar() const { return vs_; }
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *evt) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *evt) override;
	virtual void dropEvent(QDropEvent *event) override;

	virtual void keyPressEvent(QKeyEvent *evt) override;
	virtual void keyReleaseEvent(QKeyEvent *evt) override;
	virtual void leaveEvent(QEvent *evt) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void wheelEvent(QWheelEvent *evt) override;
	virtual void paintEvent(QPaintEvent *ev) override;
	virtual void resizeEvent(QResizeEvent *ev) override;

private:
	NO_ASSIGN_COPY_MOVE(IconView);
	
	void ClearDndAnimation(const QPoint &drop_coord);
	void ClearMouseOver();
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
	ElapsedTimer last_repaint_;
	bool delayed_repaint_pending_ = false;
	ci64 delay_repaint_ms_ = 100;
	bool repaint_without_delay_ = false;
	bool magnified_ = false;
	int scroll_page_step_ = -1;
	DirId last_cancelled_except_ = -1;
	ShiftSelect shift_select_ = {};
	
	bool mouse_down_ = false;
	QPoint drop_coord_ = {-1, -1};
	QPoint drag_start_pos_ = {-1, -1};
	QPoint mouse_pos_ = {-1, -1};
	int mouse_over_file_ = -1;
};



}
